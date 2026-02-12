# Integrated Motor Control & Microbot Tracking Dashboard

This system integrates motor control with ROS2-based microbot tracking, displaying everything in one unified dashboard.

## System Architecture

```
┌─────────────────────┐     ┌──────────────────────┐
│  Motor Controller   │     │  ROS2 Microbot Node  │
│   (Arduino)         │     │  (Camera Detection)  │
└──────────┬──────────┘     └──────────┬───────────┘
           │                           │
           │ Serial                    │ ROS2 Topics
           ↓                           ↓
┌─────────────────────┐     ┌──────────────────────┐
│  Motor Backend      │     │  ROS2 Web Bridge     │
│  (C++ HTTP Server)  │     │  (Python/Flask)      │
│  Port: 5173         │     │  Port: 5174          │
└──────────┬──────────┘     └──────────┬───────────┘
           │                           │
           └───────────┬───────────────┘
                       │ HTTP/MJPEG
                       ↓
           ┌───────────────────────┐
           │  Integrated Dashboard │
           │    (dashboard.html)   │
           └───────────────────────┘
```

## Prerequisites

### Motor Control Backend
- C++ compiler with C++17 support
- CMake
- Arduino with motor control firmware uploaded
- Serial port access

### ROS2 Microbot Detector
- ROS2 (Humble or later)
- Python 3
- USB camera
- Python packages:
  - rclpy
  - cv2 (OpenCV)
  - numpy
  - scipy
  - cv_bridge
  - Flask
  - flask-cors

## Installation

### 1. Setup Motor Control Backend

```bash
cd ~/Documents/one-motor-cpp
mkdir -p build && cd build
cmake ..
make
```

### 2. Setup ROS2 Microbot Detector

```bash
cd ~/Documents/object_tracking/generalized_planning_mapping_microbots/ros2_ws

# Build the ROS2 package
colcon build --packages-select microbot_detector_ros2

# Source the workspace
source install/setup.bash
```

### 3. Install Python Dependencies for ROS2 Bridge

```bash
pip install flask flask-cors opencv-python numpy scipy
```

## Running the System

### Quick Start (Recommended)

Use the automatic launcher script for easiest setup:

```bash
cd ~/Documents/one-motor-cpp
./launch_integrated_system.sh
```

The launcher will:
1. ✓ Check all prerequisites and dependencies
2. ✓ Verify motor backend is built
3. ✓ Confirm ROS2 workspace is compiled
4. ✓ Auto-detect Arduino and camera devices
5. ✓ Launch all components in tmux panes
6. ✓ Open dashboard in your browser automatically

**Tmux Navigation:**
- `Ctrl+b` then arrow keys - Switch between panes
- `Ctrl+b` then `z` - Zoom in/out of current pane
- `Ctrl+b` then `d` - Detach from session (keeps running)
- `tmux attach` - Reattach to session

### Manual Launch

If you prefer manual control, you need to run **3 terminals** simultaneously:

### Terminal 1: Motor Control Backend

```bash
cd ~/Documents/one-motor-cpp/build

# Set environment variables
export SERIAL_PORT=/dev/serial/by-id/YOUR_ARDUINO_DEVICE
export PORT=5173
export STATIC_DIR=../public

# Run the backend
./motor_control_app
```

**Note:** Replace `YOUR_ARDUINO_DEVICE` with your actual Arduino serial device path. Find it with:
```bash
ls /dev/serial/by-id/
```

### Terminal 2: ROS2 Microbot Detector

```bash
cd ~/Documents/object_tracking/generalized_planning_mapping_microbots/ros2_ws

# Source ROS2 and workspace
source /opt/ros/humble/setup.bash  # or your ROS2 distro
source install/setup.bash

# Launch the microbot detector
ros2 launch microbot_detector_ros2 microbot_detector.launch.py
```

### Terminal 3: ROS2 Web Bridge

```bash
cd ~/Documents/one-motor-cpp/backend

# Make sure ROS2 is sourced
source /opt/ros/humble/setup.bash
source ~/Documents/object_tracking/generalized_planning_mapping_microbots/ros2_ws/install/setup.bash

# Run the web bridge
python3 ros2_bridge.py
```

### Access the Dashboard

Open your browser and navigate to:
```
http://localhost:5173/dashboard.html
```

## Dashboard Features & Design

### Overview
The integrated dashboard provides a professional, enterprise-grade interface for controlling motors and monitoring microbot tracking in real-time. The layout is optimized for fitting all information on a single screen without scrolling.

### Design Philosophy
- **Professional Color Scheme**: Enterprise blue tones replacing flashy AI-generated colors
  - Primary Blue (#2563eb) - Primary actions and highlights
  - Green (#16a34a) - Success states and running indicators
  - Red (#dc2626) - Stop actions and warnings
  - Slate Grays - Backgrounds and neutral elements
- **Compact Layout**: Reduced spacing and padding to maximize screen real estate
- **Square Motor Cards**: All motor control cards maintain perfect 1:1 aspect ratio
- **High Readability**: Optimized font sizes and contrast for easy reading
- **Responsive Design**: Adapts to different screen sizes from desktop to mobile

### Left Side: Motor Control Section

#### Header
- **System Status**: Real-time connection indicators for both motor and tracking systems
  - Green indicator: System connected and operational
  - Red indicator: System offline or disconnected

#### Master Controls Panel
Control all 9 motors simultaneously from one interface:
- **Master RPM Input**: Set target RPM (0-1500) for all motors
- **Master Direction Toggle**: Choose CW (clockwise) or CCW (counter-clockwise) for all motors
- **START ALL MOTORS Button**: Execute command to all 9 motors simultaneously
  - Starts motors in parallel for instant response
  - Shows loading state during execution
  - Toast notification on completion

#### Individual Motor Cards (3x3 Grid)
Each motor card is a perfect square containing:

**Motor Header:**
- Motor ID (M1-M9)
- Status badge (ON/OFF) with color coding
  - Green badge: Motor running
  - Gray badge: Motor stopped

**Control Section:**
- **Target RPM Input**: Set individual motor target (0-1500 RPM)
  - Numeric input with validation
  - Arrow keys for adjustment
- **Direction Buttons**: Toggle between CW/CCW
  - Active button highlighted in blue
- **Action Buttons**:
  - ▶ START: Start motor with current settings
  - ⚡ SET: Update motor parameters while running
  - ⏹ STOP: Stop motor immediately

**Telemetry Section** (Real-time, updates every 200ms):
- **Measured RPM**: Actual motor speed (highlighted in blue)
- **PWM Duty**: Current PWM duty cycle value
- **Encoder Count**: Raw encoder tick count
- **Target**: Configured target RPM and direction

**Visual Feedback:**
- Running motors have green border glow
- Hover effects on all interactive elements
- Smooth transitions and animations

### Right Side: Microbot Tracking Section

#### Microbot Counter
Large, prominent display showing total number of detected microbots in real-time.

#### Camera Feeds
Two live video streams displayed vertically:
- **Debug View**: Annotated camera feed with:
  - Bounding boxes around detected microbots
  - Robot IDs overlaid
  - Grid lines showing spatial divisions
  - Occupancy counts in grid cells
- **Binary View**: Threshold image showing detection processing
  - White regions: Detected objects
  - Black background: Empty space
  - Useful for adjusting detection sensitivity

**Offline Handling:**
- Shows "Waiting for feed..." when camera not available
- Automatically reconnects when feed becomes available

#### Occupancy Grids
Visual representation of microbot spatial distribution:

**Coarse Grid (3x3):**
- Divides camera view into 9 large cells
- Shows number of microbots in each cell
- Cells with microbots highlighted in blue
- Useful for quick spatial overview

**Fine Grid (9x9):**
- Divides camera view into 81 smaller cells
- More precise spatial information
- Same color coding as coarse grid
- Better for detailed positioning

#### Detected Microbots List
Scrollable list (max 15 visible) showing:
- **Robot ID**: Unique identifier with tracking
- **Position**: Pixel coordinates (x, y)
- **Grid Assignment**:
  - C(row, col): Coarse grid cell
  - F(row, col): Fine grid cell
- **Hover Effect**: Highlights on mouse over
- Shows "+N more" indicator if more than 15 detected

### User Interaction Features

#### Real-time Updates
- Motor telemetry refreshes 5 times per second
- Microbot data updates twice per second
- Camera feeds stream at ~30 FPS
- System status checks every 2-5 seconds

#### Toast Notifications
Temporary notifications appear in bottom-right for:
- Motor start/stop confirmations
- Batch operation results
- Error messages
- Duration: 3-4 seconds

#### Keyboard Shortcuts
When OpenCV windows are open (debugging):
- `+`: Increase detection threshold
- `-`: Decrease detection threshold
- `q`: Quit detection node

#### Responsive Breakpoints
- **1400px+**: Full layout with 420px tracking panel
- **1200px**: Narrower tracking panel (360px)
- **1000px**: Single column, tracking on top
- **768px**: 3 motor columns, stacked controls
- **600px**: 2 motor columns, minimal layout

### Performance Optimizations

#### Frontend
- Efficient DOM updates using direct element access
- Debounced input validation
- Hardware-accelerated CSS animations
- Minimal re-renders

#### Backend
- Parallel API calls for batch operations
- MJPEG streaming for camera feeds
- Compressed JSON for telemetry data
- Connection pooling and reuse

### Color Coding Reference

**Status Colors:**
- 🔵 Blue: Active selections, primary actions
- 🟢 Green: Running states, success operations
- 🔴 Red: Stop actions, errors
- ⚪ Gray: Inactive states, offline systems

**Telemetry Colors:**
- Blue text: Primary measurements (RPM)
- White text: Secondary values (PWM, encoder)
- Dim gray: Labels and units

### Browser Compatibility
Tested and optimized for:
- Chrome/Chromium 90+
- Firefox 88+
- Edge 90+
- Safari 14+

**Note:** Internet Explorer is not supported due to modern CSS features.

## ROS2 Topics Published

The microbot detector publishes these topics:

```
/microbot/count                    - Int32: Total robot count
/microbot/positions                - MicrobotArray: All robot positions
/microbot/occupancy/coarse         - OccupancyGrid: 3x3 grid
/microbot/occupancy/fine           - OccupancyGrid: 9x9 grid
/microbot/debug/image              - Image: Annotated camera feed
/microbot/debug/binary             - Image: Binary threshold
/microbot/camera/raw               - Image: Raw camera feed
```

## Configuration

### Motor Control
Edit environment variables before running the motor backend.

### Microbot Detector
Edit the config file:
```bash
nano ~/Documents/object_tracking/generalized_planning_mapping_microbots/ros2_ws/src/microbot_detector_ros2/config/detector_params.yaml
```

Key parameters:
- `camera.index`: USB camera index (usually 0)
- `detection.binary_threshold`: Threshold for detection (0-255)
- `grid.coarse_rows/cols`: Coarse grid dimensions
- `grid.fine_rows/cols`: Fine grid dimensions

## Troubleshooting

### Motors Not Connecting
- Check serial port permissions: `sudo usermod -a -G dialout $USER` (logout/login required)
- Verify Arduino is connected: `ls /dev/serial/by-id/`
- Check Arduino firmware is uploaded

### Tracking System Offline
- Verify ROS2 workspace is built: `colcon build`
- Check camera is accessible: `ls /dev/video*`
- Ensure all ROS2 nodes are running: `ros2 node list`
- Check bridge is receiving data: `ros2 topic echo /microbot/count`

### Camera Feed Not Showing
- Check camera permissions
- Verify camera index in config
- Test camera with: `cheese` or `guvcview`
- Check bridge server logs for errors

### No Microbots Detected
- Adjust `binary_threshold` in config file
- Check camera focus and lighting
- View binary image to see what's being detected
- Press `+` or `-` keys in OpenCV window to adjust threshold live

## API Endpoints

### Motor Control (Port 5173)
```
GET  /api/status              - System status
GET  /api/motor/{id}/read     - Read motor telemetry
GET  /api/motor/{id}/start    - Start motor (params: rpm, dir)
GET  /api/motor/{id}/stop     - Stop motor
GET  /api/motor/{id}/set      - Set motor params (params: rpm, dir)
```

### Microbot Tracking (Port 5174)
```
GET  /api/microbot/data           - JSON data (count, positions, grids)
GET  /api/microbot/status         - Bridge status
GET  /api/microbot/events         - Server-Sent Events stream
GET  /api/microbot/stream/raw     - Raw camera MJPEG stream
GET  /api/microbot/stream/debug   - Debug view MJPEG stream
GET  /api/microbot/stream/binary  - Binary MJPEG stream
```

## Files Created/Modified

### New Files:
- `/home/ashed/Documents/one-motor-cpp/backend/ros2_bridge.py` - ROS2 to web bridge server

### Modified Files:
- `/home/ashed/Documents/one-motor-cpp/public/dashboard.html` - Integrated dashboard

## Performance Tips

1. **Reduce camera resolution** for better performance:
   - Edit `detector_params.yaml`: `camera.frame_width: 320` and `camera.frame_height: 240`

2. **Skip frames** if processing is slow:
   - Edit `detector_params.yaml`: `performance.skip_frames: 1`

3. **Disable OpenCV windows** when not debugging:
   - Edit `detector_params.yaml`: `visualization.show_opencv_windows: false`

4. **Adjust publish rate**:
   - Edit `detector_params.yaml`: `performance.publish_rate: 15`

## Quick Reference

### Common Tasks

**Start All Motors at 500 RPM CW:**
1. Open dashboard at http://localhost:5173/dashboard.html
2. Set Master RPM to `500`
3. Ensure Master Direction is `CW`
4. Click "START ALL MOTORS"

**Check Individual Motor Status:**
- Look at motor card status badge (green = ON, gray = OFF)
- Check "Measured" value in telemetry section
- Verify PWM value is non-zero when running

**Adjust Microbot Detection Sensitivity:**
1. Check binary camera feed
2. If too much noise: Increase threshold in `detector_params.yaml`
3. If missing microbots: Decrease threshold
4. Live adjustment: Focus OpenCV window, press `+` or `-`

**View ROS2 Topics:**
```bash
# List all topics
ros2 topic list

# Monitor microbot count
ros2 topic echo /microbot/count

# View position data
ros2 topic echo /microbot/positions
```

**Restart Components:**
```bash
# Restart motor backend
cd ~/Documents/one-motor-cpp/build
./motor_control_app

# Restart ROS2 detector
ros2 launch microbot_detector_ros2 microbot_detector.launch.py

# Restart web bridge
python3 ~/Documents/one-motor-cpp/backend/ros2_bridge.py
```

**Emergency Stop All Motors:**
- Click individual STOP buttons on each motor card, OR
- Send stop command to all: Open browser console and run:
  ```javascript
  for(let i=1; i<=9; i++) fetch(`/api/motor/${i}/stop`)
  ```

### Dashboard Keyboard Shortcuts

| Key | Action | Context |
|-----|--------|---------|
| `+` | Increase detection threshold | OpenCV window focused |
| `-` | Decrease detection threshold | OpenCV window focused |
| `q` | Quit detector node | OpenCV window focused |
| `Ctrl+R` | Refresh dashboard | Browser |
| `F11` | Fullscreen mode | Browser |
| `F5` | Hard refresh (clear cache) | Browser |

### File Locations

**Configuration Files:**
- Motor backend config: Environment variables (`SERIAL_PORT`, `PORT`)
- Detector config: `~/Documents/object_tracking/.../config/detector_params.yaml`
- Dashboard: `~/Documents/one-motor-cpp/public/dashboard.html`

**Log Locations:**
- Motor backend: Terminal output (stdout)
- ROS2 detector: `~/.ros/log/` directory
- Web bridge: Terminal output (stdout)

**Data Files:**
- ROS2 bag files: Record with `ros2 bag record /microbot/positions`
- Camera calibration: Stored in detector config

### Useful Commands

```bash
# Check if motors are connected
ls /dev/serial/by-id/

# Check camera devices
ls /dev/video* && v4l2-ctl --list-devices

# Monitor system resources
htop  # CPU/Memory
iftop # Network

# Check port usage
sudo netstat -tulpn | grep :5173
sudo netstat -tulpn | grep :5174

# Kill stuck processes
pkill -f motor_control_app
pkill -f ros2_bridge.py
pkill -f microbot_detector

# View ROS2 node graph
ros2 run rqt_graph rqt_graph

# Record microbot data
ros2 bag record /microbot/positions /microbot/count

# Check ROS2 bridge status
curl http://localhost:5174/api/microbot/status | jq
```

### System Requirements

**Minimum:**
- CPU: Dual-core 2.0 GHz
- RAM: 4 GB
- USB ports: 2 (Arduino + Camera)
- Display: 1280x720

**Recommended:**
- CPU: Quad-core 2.5 GHz+
- RAM: 8 GB+
- USB ports: 3 (USB hub recommended)
- Display: 1920x1080 or higher
- GPU: Optional, for OpenCV acceleration

### Support & Documentation

- **Motor Control Firmware**: See `firmware/MotorControlNine/MotorControlNine.ino`
- **ROS2 Detector**: See `microbot_detector_ros2/` package README
- **API Documentation**: This file, "API Endpoints" section
- **Issues**: Report at https://github.com/anthropics/claude-code/issues

## License

This is an integrated system combining multiple components. See individual component licenses.
