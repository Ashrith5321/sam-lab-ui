#!/bin/bash

# Integrated Motor Control & Microbot Tracking System Launcher
# This script helps launch all required components

set -e

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║   Integrated Motor Control & Microbot Tracking System         ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check if running in tmux or screen
if [ -z "$TMUX" ] && [ -z "$STY" ]; then
    echo -e "${YELLOW}⚠ Not running in tmux or screen${NC}"
    echo "This script works best with tmux for managing multiple terminals."
    echo ""
    echo "Options:"
    echo "  1) Install and use tmux (recommended)"
    echo "  2) Continue with manual terminal launching"
    echo "  3) Exit"
    echo ""
    read -p "Choose option (1-3): " choice

    case $choice in
        1)
            echo "Installing tmux..."
            sudo apt-get update && sudo apt-get install -y tmux
            echo ""
            echo "Starting tmux session..."
            tmux new-session -d -s integrated_system
            tmux send-keys -t integrated_system "$0" C-m
            tmux attach -t integrated_system
            exit 0
            ;;
        2)
            echo "Continuing with manual launch..."
            ;;
        3)
            echo "Exiting."
            exit 0
            ;;
        *)
            echo "Invalid choice. Exiting."
            exit 1
            ;;
    esac
fi

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check if port is in use
port_in_use() {
    netstat -tuln 2>/dev/null | grep -q ":$1 " || ss -tuln 2>/dev/null | grep -q ":$1 "
}

# Check prerequisites
echo -e "${BLUE}Checking prerequisites...${NC}"

if ! command_exists ros2; then
    echo -e "${RED}✗ ROS2 not found${NC}"
    echo "Please install ROS2 (Humble or later)"
    exit 1
fi
echo -e "${GREEN}✓ ROS2 found${NC}"

if ! command_exists python3; then
    echo -e "${RED}✗ Python3 not found${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Python3 found${NC}"

# Check motor control backend
MOTOR_BACKEND="$HOME/Documents/one-motor-cpp/build/motor_control_app"
if [ ! -f "$MOTOR_BACKEND" ]; then
    echo -e "${YELLOW}⚠ Motor control backend not built${NC}"
    echo "Building motor control backend..."
    cd "$HOME/Documents/one-motor-cpp"
    mkdir -p build && cd build
    cmake .. && make
    if [ $? -ne 0 ]; then
        echo -e "${RED}✗ Failed to build motor control backend${NC}"
        exit 1
    fi
fi
echo -e "${GREEN}✓ Motor control backend ready${NC}"

# Check ROS2 workspace
ROS2_WS="$HOME/Documents/object_tracking/generalized_planning_mapping_microbots/ros2_ws"
if [ ! -d "$ROS2_WS/install" ]; then
    echo -e "${YELLOW}⚠ ROS2 workspace not built${NC}"
    echo "Building ROS2 workspace..."
    cd "$ROS2_WS"
    colcon build --packages-select microbot_detector_ros2
    if [ $? -ne 0 ]; then
        echo -e "${RED}✗ Failed to build ROS2 workspace${NC}"
        exit 1
    fi
fi
echo -e "${GREEN}✓ ROS2 workspace ready${NC}"

# Check for Arduino serial port
echo ""
echo -e "${BLUE}Checking for Arduino...${NC}"
SERIAL_DEVICES=$(ls /dev/serial/by-id/ 2>/dev/null | grep -i "arduino\|usb")

if [ -z "$SERIAL_DEVICES" ]; then
    echo -e "${YELLOW}⚠ No Arduino device found in /dev/serial/by-id/${NC}"
    echo "Available serial devices:"
    ls -1 /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || echo "  None found"
    echo ""
    read -p "Enter serial port path (or press Enter to skip motor control): " SERIAL_PORT

    if [ -z "$SERIAL_PORT" ]; then
        echo -e "${YELLOW}Skipping motor control backend${NC}"
        SKIP_MOTORS=1
    fi
else
    echo "Available Arduino devices:"
    ls -1 /dev/serial/by-id/ | grep -i "arduino\|usb" | nl
    echo ""

    DEVICE_COUNT=$(echo "$SERIAL_DEVICES" | wc -l)
    if [ "$DEVICE_COUNT" -eq 1 ]; then
        SERIAL_PORT="/dev/serial/by-id/$SERIAL_DEVICES"
        echo -e "${GREEN}Using: $SERIAL_PORT${NC}"
    else
        read -p "Select device number (1-$DEVICE_COUNT): " device_num
        SERIAL_PORT="/dev/serial/by-id/$(echo "$SERIAL_DEVICES" | sed -n "${device_num}p")"
    fi
fi

# Check camera
echo ""
echo -e "${BLUE}Checking for camera...${NC}"
CAMERA_DEVICES=$(ls /dev/video* 2>/dev/null)

if [ -z "$CAMERA_DEVICES" ]; then
    echo -e "${YELLOW}⚠ No camera found${NC}"
    read -p "Continue without camera? (y/n): " skip_camera
    if [ "$skip_camera" != "y" ]; then
        exit 1
    fi
    SKIP_TRACKING=1
else
    echo "Available cameras:"
    ls -1 /dev/video* | nl
    echo -e "${GREEN}✓ Camera found${NC}"
fi

# Check if ports are available
echo ""
echo -e "${BLUE}Checking ports...${NC}"

if port_in_use 5173; then
    echo -e "${RED}✗ Port 5173 is already in use${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Port 5173 available${NC}"

if port_in_use 5174 && [ -z "$SKIP_TRACKING" ]; then
    echo -e "${RED}✗ Port 5174 is already in use${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Port 5174 available${NC}"

# Launch components
echo ""
echo -e "${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║   Starting Integrated System...                               ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""

if [ -n "$TMUX" ]; then
    # We're in tmux, create panes
    echo "Creating tmux layout..."

    # Split window for 3 panes
    tmux split-window -h
    tmux split-window -v
    tmux select-pane -t 0

    # Launch motor control backend (if not skipped)
    if [ -z "$SKIP_MOTORS" ]; then
        tmux send-keys -t 0 "cd $HOME/Documents/one-motor-cpp/build" C-m
        tmux send-keys -t 0 "export SERIAL_PORT='$SERIAL_PORT'" C-m
        tmux send-keys -t 0 "export PORT=5173" C-m
        tmux send-keys -t 0 "export STATIC_DIR=../public" C-m
        tmux send-keys -t 0 "echo -e '${GREEN}[1/3] Starting Motor Control Backend...${NC}'" C-m
        tmux send-keys -t 0 "./motor_control_app" C-m
    fi

    # Launch ROS2 detector (if not skipped)
    if [ -z "$SKIP_TRACKING" ]; then
        tmux send-keys -t 1 "source /opt/ros/humble/setup.bash" C-m
        tmux send-keys -t 1 "cd $ROS2_WS" C-m
        tmux send-keys -t 1 "source install/setup.bash" C-m
        tmux send-keys -t 1 "echo -e '${GREEN}[2/3] Starting ROS2 Microbot Detector...${NC}'" C-m
        tmux send-keys -t 1 "sleep 2" C-m
        tmux send-keys -t 1 "ros2 launch microbot_detector_ros2 microbot_detector.launch.py" C-m

        # Launch ROS2 bridge
        tmux send-keys -t 2 "source /opt/ros/humble/setup.bash" C-m
        tmux send-keys -t 2 "cd $ROS2_WS" C-m
        tmux send-keys -t 2 "source install/setup.bash" C-m
        tmux send-keys -t 2 "cd $HOME/Documents/one-motor-cpp/backend" C-m
        tmux send-keys -t 2 "echo -e '${GREEN}[3/3] Starting ROS2 Web Bridge...${NC}'" C-m
        tmux send-keys -t 2 "sleep 5" C-m
        tmux send-keys -t 2 "python3 ros2_bridge.py" C-m
    fi

    tmux select-pane -t 0

    echo ""
    echo -e "${GREEN}════════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}System launched in tmux panes!${NC}"
    echo ""
    echo "Tmux commands:"
    echo "  Ctrl+b then arrow keys - Navigate between panes"
    echo "  Ctrl+b then z          - Zoom in/out of pane"
    echo "  Ctrl+b then d          - Detach from session"
    echo "  tmux attach            - Reattach to session"
    echo ""
    echo -e "${GREEN}Opening dashboard in 8 seconds...${NC}"
    sleep 8

    if command_exists xdg-open; then
        xdg-open "http://localhost:5173/dashboard.html" 2>/dev/null &
    elif command_exists firefox; then
        firefox "http://localhost:5173/dashboard.html" 2>/dev/null &
    elif command_exists google-chrome; then
        google-chrome "http://localhost:5173/dashboard.html" 2>/dev/null &
    fi

    echo -e "${BLUE}Dashboard URL: ${GREEN}http://localhost:5173/dashboard.html${NC}"
    echo ""

else
    # Manual launch instructions
    echo "Please open 3 separate terminals and run:"
    echo ""

    if [ -z "$SKIP_MOTORS" ]; then
        echo -e "${YELLOW}Terminal 1 - Motor Control Backend:${NC}"
        echo "  cd $HOME/Documents/one-motor-cpp/build"
        echo "  export SERIAL_PORT='$SERIAL_PORT'"
        echo "  export PORT=5173"
        echo "  export STATIC_DIR=../public"
        echo "  ./motor_control_app"
        echo ""
    fi

    if [ -z "$SKIP_TRACKING" ]; then
        echo -e "${YELLOW}Terminal 2 - ROS2 Microbot Detector:${NC}"
        echo "  source /opt/ros/humble/setup.bash"
        echo "  cd $ROS2_WS"
        echo "  source install/setup.bash"
        echo "  ros2 launch microbot_detector_ros2 microbot_detector.launch.py"
        echo ""

        echo -e "${YELLOW}Terminal 3 - ROS2 Web Bridge:${NC}"
        echo "  source /opt/ros/humble/setup.bash"
        echo "  cd $ROS2_WS"
        echo "  source install/setup.bash"
        echo "  cd $HOME/Documents/one-motor-cpp/backend"
        echo "  python3 ros2_bridge.py"
        echo ""
    fi

    echo -e "${BLUE}Then open in browser: ${GREEN}http://localhost:5173/dashboard.html${NC}"
fi

echo -e "${GREEN}════════════════════════════════════════════════════════════════${NC}"
