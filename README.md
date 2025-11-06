# one-motor-cpp

A minimal C++ HTTP + serial bridge to control up to 9 DC motors on an **Arduino Leonardo** via browser or API.  
The project serves a web UI (`./public`) and communicates with the Arduino using serial commands like:

M1:START:37:CCW

yaml
Copy code

---

## Features

- Control up to 9 motors with speed (0–100%) and direction (CW/CCW)
- HTTP API and browser UI
- Native C++ HTTP server (no dependencies)
- Real-time serial communication with Arduino
- Cross-platform (tested on Ubuntu/Linux & WSL)

---

## Requirements

### System Dependencies

| Tool | Purpose | Install command (Ubuntu/Debian) |
|------|----------|--------------------------------|
| **C++17 Compiler** | Build backend | `sudo apt install g++` |
| **CMake** | Build system | `sudo apt install cmake` |
| **Make** | Build automation | `sudo apt install make` |
| **Git** | Version control | `sudo apt install git` |
| **Arduino IDE / CLI** | Flash firmware to Arduino | `sudo apt install arduino` or `arduino-cli` |

---

## Arduino Setup

1. Connect your **Arduino Leonardo** (or compatible board).
2. Identify its serial path:

   ```bash
   ls /dev/serial/by-id/
Example output:

Copy code
usb-Arduino_LLC_Arduino_Leonardo-if00
Flash the Arduino with your firmware (e.g. one_motor.ino) that handles commands like:

bash
Copy code
M{id}:START:{speed}:{dir}
M{id}:STOP
M{id}:SET:{speed}:{dir}
and replies with:

nginx
Copy code
OK
If you don’t have the firmware yet, you can create a basic sketch that parses lines over Serial and sets motor direction pins accordingly.

After flashing, ensure the board is detected:

bash
Copy code
dmesg | grep tty
Build Instructions
Clone the repo (if not already):

bash
Copy code
git clone https://github.com/Ashrith5321/sam-lab-ui.git
cd one-motor-cpp
Then build:

bash
Copy code
cmake -S . -B build
cmake --build build -j8
If you ever need to clean:

bash
Copy code
rm -rf build
cmake -S . -B build
cmake --build build -j8
This produces:

bash
Copy code
build/one_motor
Running the Server
Run with your serial device path and port:

bash
Copy code
SERIAL_PORT=/dev/serial/by-id/usb-Arduino_LLC_Arduino_Leonardo-if00 \
PORT=5173 \
STATIC_DIR=./public \
./build/one_motor
You’ll see something like:

pgsql
Copy code
Serial open at /dev/serial/by-id/usb-Arduino_LLC_Arduino_Leonardo-if00 @115200
[SERIAL←] (no READY in 3000 ms)
HTTP serving ./public on http://127.0.0.1:5173
HTTP listening on http://127.0.0.1:5173
Then open your browser at:

cpp
Copy code
http://127.0.0.1:5173
API Reference
1. Status
GET /api/status

Check connection status to Arduino:

bash
Copy code
curl "http://127.0.0.1:5173/api/status"
Response:

json
Copy code
{"status":"OK"}
2. Motor Commands
Pattern:

bash
Copy code
/api/motor/{id}/{command}?speed={0-100}&dir={CW|CCW}
Field	Description
id	Motor number (1–9)
command	start, stop, or set
speed	0–100 (integer)
dir	CW or CCW

Start motor
bash
Copy code
curl "http://127.0.0.1:5173/api/motor/1/start?speed=40&dir=CW"
Stop motor
bash
Copy code
curl "http://127.0.0.1:5173/api/motor/1/stop"
Change speed/direction
bash
Copy code
curl "http://127.0.0.1:5173/api/motor/1/set?speed=30&dir=CCW"
Example output:

pgsql
Copy code
DEBUG handler: method=GET path='/api/motor/1/start?speed=37&dir=CCW'
DEBUG match: id=1 cmd=start qs='speed=37&dir=CCW'
DEBUG kv: 'speed'='37'
DEBUG kv: 'dir'='CCW'
DEBUG parsed: speed=37 dirStr='CCW' -> CCW
[SERIAL→] M1:START:37:CCW
[SERIAL←] OK
Serial Command Format
Every message sent to Arduino follows:

css
Copy code
M{id}:{COMMAND}:{SPEED}:{DIR}
Examples:

ruby
Copy code
M1:START:45:CW
M4:SET:77:CCW
M1:STOP
Arduino should reply OK (or any text) to confirm receipt.

Test Locally Without Arduino
You can fake the serial device using socat:

bash
Copy code
sudo apt install socat
socat -d -d pty,raw,echo=0 pty,raw,echo=0
This prints something like:

swift
Copy code
PTY is /dev/pts/5
PTY is /dev/pts/6
Then run:

bash
Copy code
SERIAL_PORT=/dev/pts/5 PORT=5173 ./build/one_motor
In another terminal, monitor responses:

bash
Copy code
cat /dev/pts/6
File Structure
cpp
Copy code
one-motor-cpp/
├── CMakeLists.txt
├── README.md
├── main.cpp
├── HttpServer.cpp
├── HttpServer.hpp
├── MotorController.cpp
├── MotorController.hpp
├── public/            # frontend UI
│   ├── index.html
│   ├── script.js
│   └── style.css
└── build/             # build output
Debugging Tips
No READY message
If you see:

scss
Copy code
[SERIAL←] (no READY in 3000 ms)
It’s harmless unless your Arduino sketch expects to send “READY”. You can modify MotorController to skip waiting.

CW/CCW reversed
If motor still spins same direction for both, fix direction pin logic in Arduino code — the C++ side is correct now.

Permission denied
If serial access fails:

bash
Copy code
sudo usermod -a -G dialout $USER
newgrp dialout
Then replug the Arduino.

Developer Notes
The query-string parser in main.cpp has been fixed to correctly parse both speed and dir keys.

API and serial responses are logged in real time to stderr.

The system can easily be extended for:

Multiple Arduinos

PWM tuning

Closed-loop control with sensors

License
TBD (personal/lab use).

Quick Summary
Setup once:

bash
Copy code
sudo apt install g++ cmake make git socat arduino
Build & run:

bash
Copy code
cd ~/Documents/one-motor-cpp
cmake -S . -B build
cmake --build build -j8
SERIAL_PORT=/dev/serial/by-id/usb-Arduino_LLC_Arduino_Leonardo-if00 \
PORT=5173 ./build/one_motor
Use:

Visit http://127.0.0.1:5173 or use the curl commands above.