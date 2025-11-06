# one-motor-cpp

A minimal C++ HTTP + serial bridge to control up to 9 DC motors on an **Arduino Leonardo** via browser or API.  
The project serves a web UI (`./public`) and communicates with the Arduino using serial commands like `M1:START:37:CCW`.

---

## Features

- Control up to 9 motors with speed (0–100%) and direction (CW/CCW)
- HTTP API and browser UI
- Native C++ HTTP server (no dependencies)
- Real-time serial communication with Arduino
- Cross-platform (tested on Ubuntu/Linux & WSL)

---

## Requirements

| Tool | Purpose | Install command (Ubuntu/Debian) |
|------|----------|--------------------------------|
| C++17 Compiler | Build backend | sudo apt install g++ |
| CMake | Build system | sudo apt install cmake |
| Make | Build automation | sudo apt install make |
| Git | Version control | sudo apt install git |
| Arduino IDE / CLI | Flash firmware to Arduino | sudo apt install arduino or arduino-cli |

---

## Full Setup and Run (All Commands in One Block)

```bash
# 1. Install dependencies
sudo apt update && sudo apt install -y g++ cmake make git socat arduino

# 2. Clone repository
git clone https://github.com/Ashrith5321/sam-lab-ui.git
cd one-motor-cpp

# 3. Build
rm -rf build
cmake -S . -B build
cmake --build build -j8

# 4. Check Arduino connection
ls /dev/serial/by-id/

# Example output:
# usb-Arduino_LLC_Arduino_Leonardo-if00

# 5. Flash your Arduino manually using Arduino IDE or arduino-cli
#    Make sure it supports the following commands:
#    M{id}:START:{speed}:{dir}
#    M{id}:STOP
#    M{id}:SET:{speed}:{dir}
#    And replies with "OK"

# 6. Verify Arduino detection
dmesg | grep tty

# 7. Run server
SERIAL_PORT=/dev/serial/by-id/usb-Arduino_LLC_Arduino_Leonardo-if00 \
PORT=5173 \
STATIC_DIR=./public \
./build/one_motor

# Expected output:
# Serial open at /dev/serial/by-id/usb-Arduino_LLC_Arduino_Leonardo-if00 @115200
# [SERIAL←] (no READY in 3000 ms)
# HTTP serving ./public on http://127.0.0.1:5173
# HTTP listening on http://127.0.0.1:5173

# 8. Open the UI
# Visit http://127.0.0.1:5173

# 9. API Tests (examples)
# Check Arduino connection
curl "http://127.0.0.1:5173/api/status"

# Start motor
curl "http://127.0.0.1:5173/api/motor/1/start?speed=40&dir=CW"

# Stop motor
curl "http://127.0.0.1:5173/api/motor/1/stop"

# Change direction/speed
curl "http://127.0.0.1:5173/api/motor/1/set?speed=30&dir=CCW"

# Example debug output:
# DEBUG handler: method=GET path='/api/motor/1/start?speed=37&dir=CCW'
# DEBUG match: id=1 cmd=start qs='speed=37&dir=CCW'
# DEBUG kv: 'speed'='37'
# DEBUG kv: 'dir'='CCW'
# DEBUG parsed: speed=37 dirStr='CCW' -> CCW
# [SERIAL→] M1:START:37:CCW
# [SERIAL←] OK

# 10. Test without Arduino (optional)
socat -d -d pty,raw,echo=0 pty,raw,echo=0
# Example output:
# PTY is /dev/pts/5
# PTY is /dev/pts/6
# Then in one terminal:
SERIAL_PORT=/dev/pts/5 PORT=5173 ./build/one_motor
# And in another terminal:
cat /dev/pts/6

# 11. Fix permission errors (if needed)
sudo usermod -a -G dialout $USER
newgrp dialout
# Then replug Arduino
