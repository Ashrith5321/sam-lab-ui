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

### 9-Encoder Configuration (Full Setup)

The firmware is now configured for **ALL 9 encoders**! The default pin assignments are:

| Motor | Encoder A | Encoder B | Notes                     |
|-------|-----------|-----------|---------------------------|
| M1    | Pin 2     | Pin 3     | Default config            |
| M2    | Pin 4     | Pin 5     | Default config            |
| M3    | Pin 6     | Pin 7     | Ready for encoder         |
| M4    | Pin 8     | Pin 9     | Ready for encoder         |
| M5    | Pin 10    | Pin 11    | Ready for encoder         |
| M6    | Pin 12    | Pin 14    | Skip pin 13 (LED)         |
| M7    | Pin 15    | Pin 16    | Ready for encoder         |
| M8    | Pin 17    | Pin 20    | Skip pins 18/19 (I2C)     |
| M9    | Pin 21    | Pin 22    | Ready for encoder         |

#### Wiring All 9 Encoders

For each motor with encoder support:

1. **Encoder A** (Channel A) → Connect to assigned pin (see table above)
2. **Encoder B** (Channel B) → Connect to assigned pin (see table above)
3. **VCC** → Connect to **5V**
4. **GND** → Connect to **GND** (common ground with Teensy)

#### To Disable Encoders on Specific Motors

If you only need encoders on some motors, edit [MotorControlNine.ino:80-105](firmware/MotorControlNine/MotorControlNine.ino):

```cpp
// Set to 255 to disable encoder for that motor
uint8_t encA[10] = {255,
  2,   // M1 - enabled
  4,   // M2 - enabled
  255, // M3 - DISABLED (change to 6 to enable)
  255, // M4 - DISABLED (change to 8 to enable)
  // ... etc
};
```

The firmware automatically detects which motors have encoders (non-255 pins) and only applies closed-loop control to those motors.

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

**Two UI Options Available:**

#### 1. Classic UI (`public/index.html`)
- **File**: `public/index.html`
- **Style**: Simple, functional grid layout
- **Features**:
  - Per-motor RPM input sliders
  - Start / Set / Stop controls
  - Live telemetry display (200ms polling)
  - Real-time encoder feedback visualization

#### 2. Modern Dashboard UI (`public/dashboard.html`) ⭐ **NEW**
- **File**: `public/dashboard.html`
- **Style**: Dark theme with gradients, animations, modern design
- **Features**:
  - Beautiful gradient cards with hover effects
  - Real-time RPM progress bars
  - Status indicators with color coding
  - Smooth animations and transitions
  - Responsive grid layout
  - Modern typography and spacing
  - Live telemetry with visual feedback

**Access the UIs:**
- Classic: `http://127.0.0.1:5173/` or `http://127.0.0.1:5173/index.html`
- Modern: `http://127.0.0.1:5173/dashboard.html`

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

### Full PID Controller (Optimized for Smooth, Fast Response)

The firmware now uses **full PID control** with optimized gains for smooth and quick response.

**Current gains** (in [MotorControlNine.ino:279-283](firmware/MotorControlNine/MotorControlNine.ino)):
```cpp
kP[i] = 1.5f;   // Proportional: fast response to error
kI[i] = 0.25f;  // Integral: eliminates steady-state error
kD[i] = 0.08f;  // Derivative: damping for smoothness (reduces overshoot)
```

### How PID Works

```
Target RPM → [PID Controller] → PWM Duty Cycle → Motor
                     ↑
              Encoder Feedback
```

**PID Equation**:
```
PWM = FeedForward(target) + kP×error + kI×∫error + kD×(Δerror/Δt)
```

**Each term does:**
- **P (Proportional)**: Immediate response - bigger error = bigger correction
- **I (Integral)**: Fixes persistent offset - accumulates error over time
- **D (Derivative)**: Smoothing/damping - resists rapid changes, reduces overshoot

### Tuning Your Motors

If you need different response characteristics, edit the gains in [MotorControlNine.ino:279-283](firmware/MotorControlNine/MotorControlNine.ino):

**For FASTER response (more aggressive)**:
```cpp
kP[i] = 2.0f;   // Increase P
kI[i] = 0.35f;  // Increase I
kD[i] = 0.05f;  // Reduce D (less damping)
```

**For SMOOTHER response (less overshoot)**:
```cpp
kP[i] = 1.2f;   // Reduce P
kI[i] = 0.20f;  // Reduce I
kD[i] = 0.12f;  // Increase D (more damping)
```

**For PRECISE control (tight regulation)**:
```cpp
kP[i] = 1.5f;   // Keep P moderate
kI[i] = 0.40f;  // Increase I (stronger correction)
kD[i] = 0.08f;  // Keep D moderate
```

### Per-Motor Tuning

If different motors behave differently, you can set individual gains:

```cpp
void setupControllerGains() {
  // Motors 1-7: default gains
  for (int i = 1; i <= 7; i++) {
    kP[i] = 1.5f;
    kI[i] = 0.25f;
    kD[i] = 0.08f;
  }

  // Motor 8: needs more aggressive tuning
  kP[8] = 2.0f;
  kI[8] = 0.35f;
  kD[8] = 0.05f;

  // Motor 9: needs smoother control
  kP[9] = 1.2f;
  kI[9] = 0.20f;
  kD[9] = 0.12f;
}
```

### Testing Your Tuning

1. **Step Response Test**: Command a sudden RPM change (e.g., 0→1000 RPM)
   - Good: Reaches target in 1-2 seconds with minimal overshoot (<10%)
   - Too aggressive: Oscillates or overshoots significantly
   - Too conservative: Takes >3 seconds to reach target

2. **Load Test**: Apply load while motor is running
   - Good: RPM stays within ±5% of target
   - Needs more I: RPM drops and stays low
   - Needs more D: RPM oscillates when load is applied

3. **Direction Change**: Change direction while running
   - Good: Smooth deceleration and acceleration
   - Needs tuning: Jerky motion or stalling

### Advanced: Ziegler-Nichols Auto-Tuning

To find optimal gains automatically:

1. Set `kI = 0` and `kD = 0`
2. Increase `kP` until motor oscillates steadily
3. Record `kP_critical` and oscillation period `T`
4. Calculate gains:
   ```cpp
   kP = 0.6 × kP_critical
   kI = 1.2 × kP_critical / T
   kD = 0.075 × kP_critical × T
   ```

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

## Scaling to 9 Encoders - Complete Guide

The firmware has been **pre-configured to support all 9 encoders** out of the box! This section explains the configuration, how to wire everything, and performance considerations.

### Why This Works on Teensy 4.1

Unlike Arduino Leonardo (which has limited interrupt pins), **Teensy 4.1 supports interrupts on ALL digital pins**. This means you can connect 9 encoders without any hardware limitations.

Additionally:
- **600 MHz ARM Cortex-M7** handles 9 encoder ISRs effortlessly
- **Hardware pullups** on all pins (no external resistors needed)
- **Low interrupt latency** for accurate encoder counting

### Full Pin Assignment Table

The firmware uses these pins for all 9 encoders:

| Motor | Encoder A Pin | Encoder B Pin | PCA9685 Board | PCA Channels |
|-------|---------------|---------------|---------------|--------------|
| M1    | 2             | 3             | 0x40          | CH0, CH1     |
| M2    | 4             | 5             | 0x40          | CH2, CH3     |
| M3    | 6             | 7             | 0x40          | CH4, CH5     |
| M4    | 8             | 9             | 0x40          | CH6, CH7     |
| M5    | 10            | 11            | 0x40          | CH8, CH9     |
| M6    | 12            | 14            | 0x40          | CH10, CH11   |
| M7    | 15            | 16            | 0x40          | CH12, CH13   |
| M8    | 17            | 20            | 0x40          | CH14, CH15   |
| M9    | 21            | 22            | 0x41          | CH0, CH1     |

**Note**: Pin 13 is skipped (often has onboard LED), pins 18/19 are reserved for I2C.

### Complete Wiring Diagram

#### For Each Encoder (Motors 1-9):

```
Encoder          Teensy 4.1
--------         ----------
VCC       --->   5V (red wire)
GND       --->   GND (black wire)
Ch A      --->   See table above (yellow wire)
Ch B      --->   See table above (white wire)
```

#### Power Distribution:

```
12V Supply
   |
   ├─> Motor Driver Board (VIN)
   |
Common GND
   |
   ├─> Teensy GND
   ├─> PCA9685 GND (both boards)
   ├─> Motor Driver GND
   └─> All encoder GNDs
```

### Step-by-Step Setup for 9 Encoders

**Step 1: Flash the Firmware**
- The firmware is already configured for 9 encoders
- Upload [MotorControlNine.ino](firmware/MotorControlNine/MotorControlNine.ino) to Teensy 4.1
- No code changes needed!

**Step 2: Wire Teensy to PCA9685 Boards**
- Connect **both** PCA9685 boards to Teensy I2C (pins 18, 19)
- Set board addresses: 0x40 and 0x41 (using solder jumpers on PCA9685)

**Step 3: Connect Encoders**
- Wire each motor's encoder according to the pin table above
- Use the internal pullups (no external resistors required)
- Keep encoder wires short (<30cm) to reduce noise

**Step 4: Connect Motors**
- Connect each motor to its corresponding PCA9685 channels
- Motor 1-8 use Board 0x40, Motor 9 uses Board 0x41

**Step 5: Test**
```bash
# Flash firmware and run backend
./build/one_motor

# Open web UI
http://127.0.0.1:5173

# Test each motor individually
# Click START on Motor 1, verify encoder feedback
# Repeat for Motors 2-9
```

### Performance Considerations

#### Interrupt Load

With 9 encoders running at 1500 RPM:
- Each encoder generates ~12 pulses/rev × 9.96 gear ratio = ~120 counts/rev
- At 1500 RPM: 1500 × 120 / 60 = **3000 interrupts/second per motor**
- Total: **9 motors × 3000 = 27,000 interrupts/second**

**Teensy 4.1 handles this easily** - interrupt latency is <1µs, ISR execution time is ~2-3µs. Total CPU usage for encoders: **~8%**.

#### Control Loop Performance

- Control loop runs every 50ms (20 Hz)
- Each loop:
  - Reads 9 encoder counts
  - Computes 9 RPM values
  - Runs 9 PI controllers
  - Sends 18 PWM updates (2 per motor)

**CPU usage: <5%** - plenty of headroom for additional features.

#### Serial Communication

Sending all 9 motor states via `ENC` command:
- 9 CSV lines × ~40 bytes = 360 bytes
- At 115200 baud: ~31ms transmission time
- Recommendation: Poll at 200ms intervals (5 Hz) to avoid overwhelming serial

### Custom Pin Assignments

Want to use different pins? Edit the encoder arrays in [MotorControlNine.ino](firmware/MotorControlNine/MotorControlNine.ino):

```cpp
// Lines 80-105
uint8_t encA[10] = {255,
  2,   // M1 A - change to any available pin
  4,   // M2 A
  6,   // M3 A
  // ... etc
};

uint8_t encB[10] = {255,
  3,   // M1 B - must match motor's A channel
  5,   // M2 B
  7,   // M3 B
  // ... etc
};
```

**Available Teensy 4.1 pins** (interrupt-capable):
- Digital: 0-17, 20-33, 34-39 (avoid 18/19 for I2C, 13 has LED)
- All support interrupts, internal pullups, and 5V tolerance

After changing pins:
1. Reflash firmware
2. Update your wiring
3. No backend or UI changes needed!

### Selectively Enabling Encoders

Don't need all 9 encoders? Disable unused ones:

```cpp
// Example: Only use encoders on Motors 1, 2, and 5
uint8_t encA[10] = {255,
  2,   // M1 - enabled
  4,   // M2 - enabled
  255, // M3 - DISABLED
  255, // M4 - DISABLED
  10,  // M5 - enabled
  255, 255, 255, 255  // M6-M9 - DISABLED
};

// Same for encB array
```

Motors without encoders (255 pins) will run **open-loop** using feed-forward PWM based on target RPM.

### Testing Encoder Feedback

#### Test Individual Encoder

```bash
# In web UI or via API:
GET /api/motor/3/start?rpm=500&dir=CW

# Wait 2 seconds, then read:
GET /api/motor/3/read

# Expected response:
# M,3,492,5840,1365,500,1,1
#     ^^^           ^^^
#  measured RPM   target RPM
```

#### Test All Encoders at Once

```bash
# Start all motors at different speeds
GET /api/motor/1/start?rpm=500&dir=CW
GET /api/motor/2/start?rpm=750&dir=CW
GET /api/motor/3/start?rpm=1000&dir=CW
# ... etc

# Read all at once
GET /api/encoders

# Returns 9 CSV lines with all motor states
```

#### Verify Closed-Loop Control

1. Start motor at 1000 RPM
2. Observe measured RPM stabilizes near 1000 (±50 RPM)
3. Manually apply load (gently slow motor with finger)
4. Motor should increase PWM to maintain RPM
5. Release load - motor should decrease PWM

### Troubleshooting 9-Encoder Setup

#### Specific Motor Encoder Not Working

**Check**:
1. Encoder pins match firmware configuration
2. Encoder VCC connected to 5V
3. Common ground with Teensy
4. Hand-spin motor - counts should change in `ENC` output
5. Encoder wires not reversed (swap A and B if direction is wrong)

**Debug**:
```bash
# Upload this test sketch to verify encoder on Motor 3:
void loop() {
  Serial.print("M3: A=");
  Serial.print(digitalRead(6));
  Serial.print(" B=");
  Serial.println(digitalRead(7));
  delay(100);
}
// Spin motor by hand - should see toggling 0s and 1s
```

#### Some Motors Work, Others Don't

**Likely cause**: Wiring error or encoder failure

**Test each encoder**:
- Upload firmware
- Use `M{id}:READ` command for each motor
- Hand-spin each motor
- Verify encoder counts increment

#### Encoder Counts Wrong Direction

**Symptom**: Motor slows down when you increase target RPM

**Solution**: Swap encoder A and B wires for that motor OR flip in firmware:

```cpp
// If Motor 5 counts backward, swap A and B pins:
uint8_t encA[10] = {255, 2,4,6,8, 11, 12,15,17,21};  // was 10, now 11
uint8_t encB[10] = {255, 3,5,7,9, 10, 14,16,20,22};  // was 11, now 10
```

#### All Encoders Noisy / Erratic Counts

**Causes**:
- Electrical noise from motors
- Long encoder wires (>50cm)
- No capacitors on motor terminals

**Solutions**:
1. Add 0.1µF ceramic capacitors across motor terminals
2. Use twisted-pair or shielded cable for encoders
3. Route encoder wires away from motor power wires
4. Add 10nF capacitors on encoder A and B lines (close to Teensy)
5. Ensure solid common ground connection

### Advanced: 4x Quadrature Decoding

Current firmware uses **1x decoding** (RISING edge on A only) for lower CPU load.

For **4x higher resolution**, enable full quadrature decoding:

```cpp
// Modify ISRs to trigger on both edges, both channels:
attachInterrupt(digitalPinToInterrupt(encA[1]), isrEncA1, CHANGE);
attachInterrupt(digitalPinToInterrupt(encB[1]), isrEncB1, CHANGE);

void isrEncA1() {
  // Full quadrature state machine
  // See: https://www.pjrc.com/teensy/td_libs_Encoder.html
}
```

This increases resolution from 12 to 48 counts/motor-rev but doubles interrupt load.

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
