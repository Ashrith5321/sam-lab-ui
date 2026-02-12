#!/usr/bin/env python3
"""
ROS2 to Web Bridge Server
Subscribes to microbot detector topics and serves them via HTTP/SSE
"""

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, QoSReliabilityPolicy, QoSHistoryPolicy
from flask import Flask, Response, jsonify, send_file
from flask_cors import CORS
from sensor_msgs.msg import Image
from std_msgs.msg import Int32
import cv2
from cv_bridge import CvBridge
import json
import threading
import time
import numpy as np
import sys

# Flask app
app = Flask(__name__)
CORS(app)

# Global state
bridge_node = None
latest_data = {
    'count': 0,
    'positions': [],
    'coarse_grid': [],
    'fine_grid': [],
    'raw_frame': None,
    'debug_frame': None,
    'binary_frame': None,
    'timestamp': 0
}
data_lock = threading.Lock()


class ROS2BridgeNode(Node):
    """ROS2 Node that subscribes to microbot topics"""

    def __init__(self):
        super().__init__('ros2_web_bridge')

        self.get_logger().info("Initializing ROS2 Web Bridge...")

        self.bridge = CvBridge()

        # QoS profiles
        sensor_qos = QoSProfile(
            reliability=QoSReliabilityPolicy.BEST_EFFORT,
            history=QoSHistoryPolicy.KEEP_LAST,
            depth=10
        )

        reliable_qos = QoSProfile(
            reliability=QoSReliabilityPolicy.RELIABLE,
            history=QoSHistoryPolicy.KEEP_LAST,
            depth=10
        )

        # Import custom messages
        try:
            from microbot_detector_ros2.msg import MicrobotArray, OccupancyGrid
            self.MicrobotArray = MicrobotArray
            self.OccupancyGrid = OccupancyGrid

            # Subscribe to topics
            self.sub_count = self.create_subscription(
                Int32, '/microbot/count', self.count_callback, reliable_qos)

            self.sub_positions = self.create_subscription(
                MicrobotArray, '/microbot/positions', self.positions_callback, reliable_qos)

            self.sub_coarse = self.create_subscription(
                OccupancyGrid, '/microbot/occupancy/coarse', self.coarse_callback, reliable_qos)

            self.sub_fine = self.create_subscription(
                OccupancyGrid, '/microbot/occupancy/fine', self.fine_callback, reliable_qos)

            self.sub_debug = self.create_subscription(
                Image, '/microbot/debug/image', self.debug_callback, sensor_qos)

            self.sub_binary = self.create_subscription(
                Image, '/microbot/debug/binary', self.binary_callback, sensor_qos)

            self.sub_raw = self.create_subscription(
                Image, '/microbot/camera/raw', self.raw_callback, sensor_qos)

            self.get_logger().info("ROS2 Web Bridge initialized successfully!")

        except ImportError as e:
            self.get_logger().error(f"Failed to import custom messages: {e}")
            self.get_logger().error("Make sure microbot_detector_ros2 is built and sourced")
            raise

    def count_callback(self, msg):
        with data_lock:
            latest_data['count'] = msg.data
            latest_data['timestamp'] = time.time()

    def positions_callback(self, msg):
        with data_lock:
            positions = []
            for bot in msg.microbots:
                positions.append({
                    'id': bot.robot_id,
                    'x': bot.x,
                    'y': bot.y,
                    'norm_x': bot.normalized_x,
                    'norm_y': bot.normalized_y,
                    'coarse_row': bot.coarse_grid_row,
                    'coarse_col': bot.coarse_grid_col,
                    'fine_row': bot.fine_grid_row,
                    'fine_col': bot.fine_grid_col,
                    'area': bot.area
                })
            latest_data['positions'] = positions
            latest_data['timestamp'] = time.time()

    def coarse_callback(self, msg):
        with data_lock:
            grid = np.array(msg.data).reshape(msg.grid_rows, msg.grid_cols)
            latest_data['coarse_grid'] = grid.tolist()
            latest_data['timestamp'] = time.time()

    def fine_callback(self, msg):
        with data_lock:
            grid = np.array(msg.data).reshape(msg.grid_rows, msg.grid_cols)
            latest_data['fine_grid'] = grid.tolist()
            latest_data['timestamp'] = time.time()

    def raw_callback(self, msg):
        try:
            frame = self.bridge.imgmsg_to_cv2(msg, "bgr8")
            with data_lock:
                latest_data['raw_frame'] = frame
        except Exception as e:
            self.get_logger().error(f"Error converting raw image: {e}")

    def debug_callback(self, msg):
        try:
            frame = self.bridge.imgmsg_to_cv2(msg, "bgr8")
            with data_lock:
                latest_data['debug_frame'] = frame
        except Exception as e:
            self.get_logger().error(f"Error converting debug image: {e}")

    def binary_callback(self, msg):
        try:
            frame = self.bridge.imgmsg_to_cv2(msg, "mono8")
            with data_lock:
                latest_data['binary_frame'] = frame
        except Exception as e:
            self.get_logger().error(f"Error converting binary image: {e}")


def generate_mjpeg(frame_key):
    """Generate MJPEG stream for a specific frame type"""
    while True:
        with data_lock:
            frame = latest_data.get(frame_key)

        if frame is not None:
            # Encode frame as JPEG
            ret, buffer = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, 85])
            if ret:
                frame_bytes = buffer.tobytes()
                yield (b'--frame\r\n'
                       b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')

        time.sleep(0.033)  # ~30 FPS


@app.route('/api/microbot/stream/raw')
def stream_raw():
    """Stream raw camera feed"""
    return Response(generate_mjpeg('raw_frame'),
                    mimetype='multipart/x-mixed-replace; boundary=frame')


@app.route('/api/microbot/stream/debug')
def stream_debug():
    """Stream debug visualization"""
    return Response(generate_mjpeg('debug_frame'),
                    mimetype='multipart/x-mixed-replace; boundary=frame')


@app.route('/api/microbot/stream/binary')
def stream_binary():
    """Stream binary thresholded image"""
    return Response(generate_mjpeg('binary_frame'),
                    mimetype='multipart/x-mixed-replace; boundary=frame')


@app.route('/api/microbot/data')
def get_data():
    """Get all microbot data as JSON"""
    with data_lock:
        return jsonify({
            'count': latest_data['count'],
            'positions': latest_data['positions'],
            'coarse_grid': latest_data['coarse_grid'],
            'fine_grid': latest_data['fine_grid'],
            'timestamp': latest_data['timestamp']
        })


@app.route('/api/microbot/status')
def get_status():
    """Get bridge status"""
    with data_lock:
        age = time.time() - latest_data['timestamp']
        has_frames = (latest_data['debug_frame'] is not None and
                      latest_data['binary_frame'] is not None)

        return jsonify({
            'connected': age < 2.0,
            'has_frames': has_frames,
            'microbot_count': latest_data['count'],
            'last_update': age
        })


@app.route('/api/microbot/events')
def events():
    """Server-Sent Events stream for real-time updates"""
    def event_stream():
        last_sent = 0
        while True:
            with data_lock:
                if latest_data['timestamp'] > last_sent:
                    data = {
                        'count': latest_data['count'],
                        'positions': latest_data['positions'],
                        'coarse_grid': latest_data['coarse_grid'],
                        'fine_grid': latest_data['fine_grid']
                    }
                    last_sent = latest_data['timestamp']
                    yield f"data: {json.dumps(data)}\n\n"
            time.sleep(0.1)

    return Response(event_stream(), mimetype='text/event-stream')


def run_ros2():
    """Run ROS2 node in separate thread"""
    global bridge_node

    rclpy.init()
    bridge_node = ROS2BridgeNode()

    try:
        rclpy.spin(bridge_node)
    except KeyboardInterrupt:
        pass
    finally:
        bridge_node.destroy_node()
        rclpy.shutdown()


def main():
    """Main entry point"""
    print("Starting ROS2 Web Bridge...")
    print("=" * 60)

    # Start ROS2 node in separate thread
    ros_thread = threading.Thread(target=run_ros2, daemon=True)
    ros_thread.start()

    # Wait a bit for ROS2 to initialize
    time.sleep(2)

    # Start Flask server
    print("Flask server starting on http://localhost:5174")
    print("Endpoints:")
    print("  GET /api/microbot/data         - JSON data")
    print("  GET /api/microbot/status       - Bridge status")
    print("  GET /api/microbot/events       - SSE stream")
    print("  GET /api/microbot/stream/raw   - Raw camera MJPEG")
    print("  GET /api/microbot/stream/debug - Debug view MJPEG")
    print("  GET /api/microbot/stream/binary - Binary MJPEG")
    print("=" * 60)

    app.run(host='0.0.0.0', port=5174, threaded=True, debug=False)


if __name__ == '__main__':
    main()
