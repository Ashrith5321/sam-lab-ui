SAM Lab Motor Control UI (RPM + Encoder Support)

A minimal C++ HTTP + Serial control stack for controlling up to 9 DC motors from a browser or API, with closed-loop RPM control using encoders (currently enabled for a subset of motors).

This project is designed for robotics lab use, prioritizing:

deterministic behavior

clear hardware/software separation

scalability to more capable microcontrollers

🚀 Features
Core

Control up to 9 DC motors

Browser-based UI + REST API

Native C++ HTTP server (no external dependencies)

Real-time serial communication with microcontroller

Cross-platform (Ubuntu/Linux, WSL tested)

RPM + Encoder (NEW)

Command motors using RPM instead of voltage/speed %

Closed-loop PI RPM control using quadrature encoders

Live encoder telemetry (measured RPM, counts, PWM)

Max supported RPM: 1500 RPM (output shaft)

⚠️ Important Hardware Note (READ THIS)

Due to Arduino Leonardo (ATmega32u4) interrupt limitations:

✅ RPM commands are supported for all 9 motors

✅ Encoder feedback + closed-loop control are enabled for Motor 1 and Motor 2

❌ Motors 3–9 currently run open-loop (feed-forward only)

This is an intentional design decision to ensure reliability at high RPM.

If you require encoder feedback on more motors, see Scaling Options below.

🧠 Control Architecture
Open-loop motors (no encoder)
RPM command → feed-forward PWM → motor

Closed-loop motors (with encoder)
RPM command → PI controller → PWM → motor
                   ↑
               encoder feedback

🔧 Hardware Setup
Motor

Pololu 10:1 Micro Metal Gearmotor (HPCB 12V)

Encoder: 12 CPR quadrature

Gear ratio: 9.96:1

Effective counts/output-rev ≈ 240

Power

Motors powered from external 12V supply

Arduino must NOT power motors

All grounds must be common

Encoder Wiring (Motors 1 & 2)
Signal	Motor 1	Motor 2
Encoder A	D2	D3
Encoder B	D4	D5
VCC	5V	5V
GND	GND	GND
📦 Software Components
Arduino Firmware

Encoder pulse counting (interrupt-based)

RPM computation

PI control loop (50 ms)

Serial protocol for commands + telemetry

Backend (C++)

Serial bridge to Arduino

Motor abstraction using RPM

REST API + static file server

Frontend (HTML/JS)

Per-motor RPM input

Start / Set / Stop buttons

Live telemetry polling (200 ms)

🖥️ API Endpoints
Status
GET /api/status

Motor Control
GET /api/motor/{id}/start?rpm=1000&dir=CW
GET /api/motor/{id}/set?rpm=1200&dir=CCW
GET /api/motor/{id}/stop

Telemetry
GET /api/motor/{id}/read
GET /api/encoders

🧪 Example Serial Commands (Arduino side)
M1:START:1000:CW
M1:SET:1200:CCW
M1:STOP
M1:READ
ENC

🛠️ Build & Run
Install dependencies
sudo apt update && sudo apt install -y g++ cmake make git socat arduino

Clone
git clone https://github.com/Ashrith5321/sam-lab-ui.git
cd sam-lab-ui

Build
rm -rf build
cmake -S . -B build
cmake --build build -j8

Run
SERIAL_PORT=/dev/serial/by-id/usb-Arduino_LLC_Arduino_Leonardo-if00 \
PORT=5173 \
STATIC_DIR=./public \
./build/one_motor


Open:

http://127.0.0.1:5173

🧪 Test Without Hardware (Optional)
socat -d -d pty,raw,echo=0 pty,raw,echo=0


Terminal 1:

SERIAL_PORT=/dev/pts/5 PORT=5173 ./build/one_motor


Terminal 2:

cat /dev/pts/6

📈 Scaling Options (Future Work)

If you need encoder feedback on all motors:

Recommended

Teensy 4.1 (hardware quadrature decoding)

ESP32 (PCNT units)

Alternatives

External encoder counters (SPI/I²C)

Magnetic encoders

Backend + UI do not need to change for these upgrades.

🧑‍🔬 Intended Use

This project is intended for:

Robotics research & labs

Motor characterization

Control systems experimentation

Educational platforms