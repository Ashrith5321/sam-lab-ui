#!/usr/bin/env python3
"""
Navigation Planner for Microbot Motor Grid
Routes microbots from one motor to another by activating motors in sequence.
Uses BFS pathfinding on a 3x3 motor grid.

Motor layout:        Grid positions (row, col):
  1  4  7              (0,0) (0,1) (0,2)
  2  5  8              (1,0) (1,1) (1,2)
  3  6  9              (2,0) (2,1) (2,2)

Usage:
  CLI:  python nav_planner.py --from 1 --to 9 --rpm 500
  API:  python nav_planner.py --server
"""

import argparse
import json
import threading
import time
from collections import deque

import requests
from flask import Flask, Response, jsonify, request
from flask_cors import CORS

# --- Configuration ---
MOTOR_API = "http://localhost:5173"
MICROBOT_API = "http://localhost:5174"
PLANNER_PORT = 5175

POLL_INTERVAL = 0.5   # seconds between arrival checks
WAYPOINT_TIMEOUT = 30  # seconds max wait per waypoint
DEFAULT_RPM = 500

# --- Motor grid mapping ---
# motor_id -> (row, col)
MOTOR_GRID = {
    1: (0, 0), 4: (0, 1), 7: (0, 2),
    2: (1, 0), 5: (1, 1), 8: (1, 2),
    3: (2, 0), 6: (2, 1), 9: (2, 2),
}

# Reverse: (row, col) -> motor_id
GRID_TO_MOTOR = {v: k for k, v in MOTOR_GRID.items()}


def get_neighbors(motor_id):
    """Get adjacent motor IDs (4-connected: up, down, left, right)."""
    r, c = MOTOR_GRID[motor_id]
    neighbors = []
    for dr, dc in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
        nr, nc = r + dr, c + dc
        if (nr, nc) in GRID_TO_MOTOR:
            neighbors.append(GRID_TO_MOTOR[(nr, nc)])
    return neighbors


def find_path(src, dst):
    """BFS shortest path from src motor to dst motor. Returns list of motor IDs."""
    if src == dst:  
        return [src]
    queue = deque([[src]])
    visited = {src}
    while queue:
        path = queue.popleft()
        for neighbor in get_neighbors(path[-1]):
            if neighbor == dst:
                return path + [dst]
            if neighbor not in visited:
                visited.add(neighbor)
                queue.append(path + [neighbor])
    return []  # no path (shouldn't happen on connected 3x3 grid)


def activate_motor(motor_id, rpm=DEFAULT_RPM):
    """Start a motor via the C++ HTTP API."""
    url = f"{MOTOR_API}/api/motor/{motor_id}/start?rpm={rpm}&dir=CW"
    try:
        r = requests.get(url, timeout=5)
        return r.status_code == 200
    except requests.RequestException as e:
        print(f"  [ERROR] Failed to activate motor {motor_id}: {e}")
        return False


def deactivate_motor(motor_id):
    """Stop a motor via the C++ HTTP API."""
    url = f"{MOTOR_API}/api/motor/{motor_id}/stop"
    try:
        r = requests.get(url, timeout=5)
        return r.status_code == 200
    except requests.RequestException as e:
        print(f"  [ERROR] Failed to deactivate motor {motor_id}: {e}")
        return False


def check_arrival(motor_id):
    """Check if microbots have arrived at the motor's grid cell.
    Uses the coarse 3x3 occupancy grid from the ROS2 bridge.
    """
    r, c = MOTOR_GRID[motor_id]
    try:
        resp = requests.get(f"{MICROBOT_API}/api/microbot/data", timeout=5)
        data = resp.json()
        coarse = data.get("coarse_grid", [])
        if coarse and r < len(coarse) and c < len(coarse[r]):
            return coarse[r][c] > 0
    except requests.RequestException:
        pass
    return False


# --- Navigation state (for API mode) ---
nav_state = {
    "status": "idle",       # idle | running | completed | cancelled | error
    "path": [],
    "current_step": 0,
    "current_motor": None,
    "message": "",
    "rpm": DEFAULT_RPM,
}
nav_lock = threading.Lock()
cancel_flag = threading.Event()


def navigate(src, dst, rpm=DEFAULT_RPM, use_state=False):
    """Navigate microbots from src motor to dst motor."""
    path = find_path(src, dst)
    if not path:
        msg = f"No path from motor {src} to motor {dst}"
        print(msg)
        if use_state:
            with nav_lock:
                nav_state["status"] = "error"
                nav_state["message"] = msg
        return False

    print(f"Path: {' -> '.join(f'M{m}' for m in path)}")

    if use_state:
        with nav_lock:
            nav_state["status"] = "running"
            nav_state["path"] = path
            nav_state["current_step"] = 0
            nav_state["rpm"] = rpm
            nav_state["message"] = f"Navigating: {' -> '.join(f'M{m}' for m in path)}"

    for i, motor_id in enumerate(path):
        if cancel_flag.is_set():
            print("  Navigation cancelled.")
            deactivate_motor(motor_id)
            if use_state:
                with nav_lock:
                    nav_state["status"] = "cancelled"
                    nav_state["message"] = f"Cancelled at motor {motor_id}"
            return False

        if use_state:
            with nav_lock:
                nav_state["current_step"] = i
                nav_state["current_motor"] = motor_id
                nav_state["message"] = f"Activating motor {motor_id} (step {i+1}/{len(path)})"

        print(f"  Step {i+1}/{len(path)}: Activating motor {motor_id}...")
        activate_motor(motor_id, rpm)

        # Wait for microbots to arrive
        start_time = time.time()
        arrived = False
        while time.time() - start_time < WAYPOINT_TIMEOUT:
            if cancel_flag.is_set():
                break
            if check_arrival(motor_id):
                arrived = True
                break
            time.sleep(POLL_INTERVAL)

        if arrived:
            print(f"  Motor {motor_id}: Microbots arrived!")
        else:
            if cancel_flag.is_set():
                deactivate_motor(motor_id)
                if use_state:
                    with nav_lock:
                        nav_state["status"] = "cancelled"
                        nav_state["message"] = f"Cancelled at motor {motor_id}"
                return False
            print(f"  Motor {motor_id}: Timeout ({WAYPOINT_TIMEOUT}s) - moving on")

        # Deactivate before moving to next waypoint
        deactivate_motor(motor_id)
        time.sleep(0.3)  # brief pause between steps

    print("Navigation complete!")
    if use_state:
        with nav_lock:
            nav_state["status"] = "completed"
            nav_state["current_step"] = len(path)
            nav_state["message"] = f"Navigation complete: M{src} -> M{dst}"
    return True


# --- Flask API ---
app = Flask(__name__)
CORS(app)


@app.route("/api/navigate", methods=["POST"])
def start_navigation():
    """Start navigation. JSON body: {"from": 1, "to": 9, "rpm": 500}"""
    with nav_lock:
        if nav_state["status"] == "running":
            return jsonify({"error": "Navigation already in progress"}), 409

    data = request.get_json(force=True)
    src = data.get("from")
    dst = data.get("to")
    rpm = data.get("rpm", DEFAULT_RPM)

    if src not in MOTOR_GRID or dst not in MOTOR_GRID:
        return jsonify({"error": "Invalid motor ID (1-9)"}), 400

    path = find_path(src, dst)
    cancel_flag.clear()

    thread = threading.Thread(target=navigate, args=(src, dst, rpm, True), daemon=True)
    thread.start()

    return jsonify({"ok": True, "path": path})


@app.route("/api/navigate/status")
def navigation_status():
    """Get current navigation state."""
    with nav_lock:
        return jsonify(nav_state)


@app.route("/api/navigate/cancel", methods=["POST"])
def cancel_navigation():
    """Cancel running navigation."""
    cancel_flag.set()
    return jsonify({"ok": True})


@app.route("/api/navigate/path")
def get_path():
    """Preview path without executing. Query params: from, to."""
    src = request.args.get("from", type=int)
    dst = request.args.get("to", type=int)
    if src not in MOTOR_GRID or dst not in MOTOR_GRID:
        return jsonify({"error": "Invalid motor ID (1-9)"}), 400
    path = find_path(src, dst)
    return jsonify({"path": path, "steps": len(path)})


def main():
    parser = argparse.ArgumentParser(description="Microbot Navigation Planner")
    parser.add_argument("--server", action="store_true", help="Run as HTTP server on port 5175")
    parser.add_argument("--from", dest="src", type=int, help="Source motor ID (1-9)")
    parser.add_argument("--to", dest="dst", type=int, help="Destination motor ID (1-9)")
    parser.add_argument("--rpm", type=int, default=DEFAULT_RPM, help=f"Motor RPM (default: {DEFAULT_RPM})")
    args = parser.parse_args()

    if args.server:
        print(f"Navigation Planner API starting on http://localhost:{PLANNER_PORT}")
        print("Endpoints:")
        print("  POST /api/navigate          - Start navigation")
        print("  GET  /api/navigate/status    - Navigation status")
        print("  POST /api/navigate/cancel    - Cancel navigation")
        print("  GET  /api/navigate/path?from=X&to=Y - Preview path")
        app.run(host="0.0.0.0", port=PLANNER_PORT, threaded=True, debug=False)
    elif args.src and args.dst:
        if args.src not in MOTOR_GRID or args.dst not in MOTOR_GRID:
            print("Error: Motor IDs must be 1-9")
            return
        navigate(args.src, args.dst, args.rpm)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
