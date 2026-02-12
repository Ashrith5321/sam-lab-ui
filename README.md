# DC Motor Control System with Encoder Feedback

A high-performance C++ HTTP + Serial control stack for controlling up to 9 DC motors from a browser or REST API, featuring closed-loop RPM control with quadrature encoder feedback.

Built for robotics research, this system prioritizes deterministic behavior, clear hardware/software separation, and real-time performance.

---

## Features

### Core Capabilities
- **Control up to 9 DC motors** via dual PCA9685 PWM drivers
- **Browser-based UI + REST API** for remote control
- **Native C++ HTTP server** (no external dependencies)
- **Real-time serial communication** with Teensy 4.1 microcontroller
- **Cross-platform** (Linux, Ubuntu, WSL tested)

### Encoder + Closed-Loop Control
- **Command motors using RPM** (not voltage or duty cycle)
- **PI closed-loop control** for precise speed regulation
- **Live encoder telemetry**: measured RPM, encoder counts, PWM duty
- **Configurable encoder support**: currently enabled for Motors 1 & 2
- **Max supported RPM**: 1500 RPM (output shaft after gearbox)

### Control Modes
- **Closed-loop (with encoder)**: RPM command → PI controller → PWM → motor ← encoder feedback
- **Open-loop (no encoder)**: RPM command → feed-forward PWM → motor

---

## Hardware

### Microcontroller: Teensy 4.1

**Why Teensy 4.1?**
- ✅ **All pins support interrupts** (no encoder pin limitations)
- ✅ **600 MHz ARM Cortex-M7** processor for fast control loops
- ✅ **Dedicated I2C pins**: SDA=18, SCL=19 (no conflicts)
- ✅ **Robust USB serial** communication
- ✅ **3.3V logic** with 5V tolerant inputs

### Motors & Encoders

**Motor**: Pololu 10:1 Micro Metal Gearmotor HPCB (12V)
- Integrated 12 CPR quadrature encoder
- Gear ratio: 9.96:1
- Effective resolution: ~240 counts per output shaft revolution

### PWM Drivers

**Two PCA9685 16-channel PWM boards**:
- Board 1: I2C address `0x40` (controls Motors 1-8)
- Board 2: I2C address `0x41` (controls Motor 9)
- PWM frequency: 1600 Hz
- Each motor uses 2 channels (IN1, IN2 for H-bridge control)

---

## Wiring Guide

### Teensy 4.1 to PCA9685 (Both Boards)

| Signal | Teensy Pin | PCA9685 Pin |
|--------|-----------|-------------|
| 5V     | 5V        | VCC         |
| GND    | GND       | GND         |
| SDA    | 18        | SDA         |
| SCL    | 19        | SCL         |

### Encoder Wiring (Motors 1 & 2)

| Signal     | Motor 1 | Motor 2 | Notes              |
|------------|---------|---------|-------------------|
| Encoder A  | Pin 2   | Pin 4   | Interrupt capable |
| Encoder B  | Pin 3   | Pin 5   | Interrupt capable |
| VCC        | 5V      | 5V      | Encoder power     |
| GND        | GND     | GND     | Common ground     |

### Motor Power

⚠️ **CRITICAL**: Motors must be powered from an **external 12V supply**
- **DO NOT** power motors from Teensy
- **MUST** connect all grounds together (Teensy, PCA9685, motor driver, encoders)
- Add 0.1µF capacitors across motor terminals to reduce EMI

### Adding More Encoders

To enable encoders on Motors 3-9, edit `firmware/MotorControlNine/MotorControlNine.ino`:

1. Add encoder pins to `encA[]` and `encB[]` arrays (lines 80-90)
2. Create new ISR functions (see `isrEncA1()` and `isrEncA2()` as examples, lines 154-163)
3. Attach interrupts in `attachEncoders()` (lines 169-182)

All Teensy 4.1 digital pins support interrupts, so you can use any available pins (avoid 18/19 for I2C).

---

## Software Architecture

### Components

```
┌─────────────┐         ┌──────────────┐         ┌─────────────┐
│  Browser UI │ ◄─HTTP─►│  C++ Backend │ ◄─Serial─►│ Teensy 4.1  │
│  (HTML/JS)  │         │   (REST API) │         │  (Firmware) │
└─────────────┘         └──────────────┘         └─────────────┘
                                                         │
                                                    I2C  │
                                                         ▼
                                                  ┌──────────┐
                                                  │ PCA9685  │
                                                  │  Boards  │
                                                  └──────────┘
                                                         │
                                                     PWM │
                                                         ▼
                                                  ┌──────────┐
                                                  │  Motors  │
                                                  │  + Enc   │
                                                  └──────────┘
```

### Firmware (Arduino/Teensy)
- **File**: `firmware/MotorControlNine/MotorControlNine.ino`
- **Features**:
  - Quadrature encoder decoding (RISING edge interrupts)
  - RPM calculation from encoder counts
  - PI control loop (50ms update rate)
  - Serial command protocol
  - Multi-motor state management

### Backend (C++)
- **Files**: `backend/MotorController.{cpp,hpp}`, `backend/HttpServer.{cpp,hpp}`
- **Features**:
  - Serial bridge to Teensy
  - REST API endpoints
  - Static file server for web UI
  - Real-time telemetry collection

### Frontend (HTML/JS)
- **File**: `public/index.html`
- **Features**:
  - Per-motor RPM input sliders
  - Start / Set / Stop controls
  - Live telemetry display (200ms polling)
  - Real-time encoder feedback visualization

---

## API Reference

### Status Check
```http
GET /api/status
```
Returns: `STATUS OK` if Teensy is connected

### Motor Control

**Start Motor**
```http
GET /api/motor/{id}/start?rpm=1000&dir=CW
```
- `id`: Motor ID (1-9)
- `rpm`: Target RPM (0-1500)
- `dir`: Direction (`CW` or `CCW`)

**Update Running Motor**
```http
GET /api/motor/{id}/set?rpm=1200&dir=CCW
```

**Stop Motor**
```http
GET /api/motor/{id}/stop
```

### Telemetry

**Single Motor**
```http
GET /api/motor/{id}/read
```
Returns CSV: `M,id,rpm,count,pwm,tgt,dir,en`

**All Motors**
```http
GET /api/encoders
```
Returns 9 lines of CSV (one per motor)

---

## Build & Run

### Prerequisites

```bash
sudo apt update && sudo apt install -y g++ cmake make git
```

For Teensy programming, install [Teensyduino](https://www.pjrc.com/teensy/td_download.html) or use Arduino IDE with Teensy support.

### Clone Repository

```bash
git clone https://github.com/Ashrith5321/sam-lab-ui.git
cd sam-lab-ui
```

### Build Backend

```bash
rm -rf build
cmake -S . -B build
cmake --build build -j$(nproc)
```

### Flash Teensy

1. Open `firmware/MotorControlNine/MotorControlNine.ino` in Arduino IDE
2. Select **Tools → Board → Teensy 4.1**
3. Select **Tools → USB Type → Serial**
4. Upload to Teensy

### Run Backend

```bash
SERIAL_PORT=/dev/ttyACM0 \
PORT=5173 \
STATIC_DIR=./public \
./build/one_motor
```

**Find your serial port**:
```bash
ls /dev/ttyACM* /dev/ttyUSB*
# or use by-id for stability:
ls /dev/serial/by-id/
```

### Access Web UI

Open browser to: **http://127.0.0.1:5173**

---

## Serial Protocol (Teensy Commands)

All commands are newline-terminated ASCII strings.

### Commands

| Command                  | Description                      | Response      |
|--------------------------|----------------------------------|---------------|
| `STATUS`                 | Check if firmware is ready       | `STATUS OK`   |
| `ENC`                    | Read all 9 motors (CSV)          | 9 lines       |
| `M{id}:START:{rpm}:{dir}`| Start motor with RPM + direction | `OK`          |
| `M{id}:SET:{rpm}:{dir}`  | Update running motor             | `OK`          |
| `M{id}:STOP`             | Stop motor                       | `OK`          |
| `M{id}:READ`             | Read single motor telemetry      | CSV line      |

### Examples

```
M1:START:1000:CW       → Start Motor 1 at 1000 RPM clockwise
M2:SET:1200:CCW        → Change Motor 2 to 1200 RPM counter-clockwise
M1:STOP                → Stop Motor 1
M3:READ                → Read Motor 3 status
ENC                    → Read all motors
```

### CSV Telemetry Format

```
M,{id},{measured_rpm},{encoder_count},{pwm_duty},{target_rpm},{dir},{enabled}
```

Example:
```
M,1,985,12450,2048,1000,1,1
```
- Motor 1, measured 985 RPM, 12450 encoder counts, PWM=2048, target=1000, CW, enabled

---

## Testing Without Hardware

Use `socat` to create virtual serial ports for testing:

```bash
socat -d -d pty,raw,echo=0 pty,raw,echo=0
```

This creates two linked pseudo-terminals (e.g., `/dev/pts/5` ↔ `/dev/pts/6`).

**Terminal 1** (Backend):
```bash
SERIAL_PORT=/dev/pts/5 PORT=5173 STATIC_DIR=./public ./build/one_motor
```

**Terminal 2** (Monitor):
```bash
cat /dev/pts/6
```

Now you can test the backend without a physical Teensy.

---

## Control Tuning

### PI Controller Gains

Default gains (in `MotorControlNine.ino`, lines 186-188):
```cpp
kP[i] = 0.9f;   // Proportional gain
kI[i] = 0.15f;  // Integral gain
```

**Tuning tips**:
- Increase `kP` for faster response (may cause oscillation)
- Increase `kI` to eliminate steady-state error (may cause overshoot)
- Test with step changes in target RPM

### Encoder Configuration

Current setup uses **1x decoding** (RISING edge only):
- Lower ISR load and noise sensitivity
- Adequate resolution for 12 CPR encoders

For higher resolution, switch to **4x decoding** (both edges, both channels).

---

## Troubleshooting

### Serial Connection Issues

**Symptom**: No `READY` message on connection

**Solutions**:
- Teensy doesn't auto-reset on serial open (this is normal)
- Backend automatically sends `STATUS` command to verify connection
- Check USB cable and port permissions: `sudo chmod 666 /dev/ttyACM0`

### Motor Not Responding

**Check**:
1. PCA9685 boards powered (5V LED should be on)
2. I2C addresses correct (0x40, 0x41) - use `i2cdetect -y 1` on Raspberry Pi
3. Motor driver board powered externally (12V)
4. Common ground between all components
5. Motor cables not reversed

### Encoder Not Reading

**Check**:
1. Encoder VCC connected to 5V
2. Common ground with Teensy
3. Encoder pins defined in `encA[]` and `encB[]` arrays
4. ISR functions created and attached
5. Try hand-spinning motor - should see counts change with `ENC` command

### RPM Oscillation

**Symptoms**: Motor speed oscillates around target

**Solutions**:
- Reduce `kP` gain
- Reduce `kI` gain
- Increase control loop period `CTRL_DT_MS` (line 76)
- Check for mechanical binding or excessive load

---

## Project Structure

```
.
├── backend/
│   ├── main.cpp              # Entry point
│   ├── HttpServer.{cpp,hpp}  # HTTP server + routing
│   ├── MotorController.{cpp,hpp}  # Serial communication
│   └── SerialPort.{cpp,hpp}  # Low-level serial I/O
├── firmware/
│   └── MotorControlNine/
│       └── MotorControlNine.ino  # Teensy firmware
├── public/
│   └── index.html            # Web UI
├── CMakeLists.txt            # Build configuration
└── README.md                 # This file
```

---

## Future Enhancements

### Planned Features
- [ ] WebSocket support for lower-latency telemetry
- [ ] Trajectory planning (acceleration/deceleration ramps)
- [ ] Auto-tuning for PI gains
- [ ] Data logging to CSV/JSON
- [ ] Multi-motor synchronization

### Hardware Scalability

**To enable encoders on all 9 motors**:
- Teensy 4.1 supports this natively (all pins interrupt-capable)
- Simply add encoder pins to firmware configuration
- No backend or UI changes required

**Alternative microcontrollers**:
- ESP32 (PCNT hardware encoder units)
- STM32 with hardware quadrature decoders
- External encoder counters (LS7366R via SPI)

---

## Use Cases

This system is designed for:
- Robotics research and experimentation
- Motor characterization and testing
- Control systems education
- Multi-actuator coordination
- Precision motion control

---

## License

MIT License - see repository for details.

---

## Contributing

Issues and pull requests welcome! Please maintain:
- Clean separation between firmware/backend/frontend
- Backward compatibility with existing hardware
- Clear documentation of hardware requirements

---

## Credits

Built for robotics lab automation and educational use.

**Hardware**: Pololu motors, Adafruit PCA9685, PJRC Teensy 4.1
**Libraries**: Adafruit_PWMServoDriver, Wire (I2C)
