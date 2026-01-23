// MotorControlNine.ino
// 9-motor PWM + CLOSED-LOOP RPM control (best-effort) for Arduino Leonardo.
//
// CRITICAL LEONARDO NOTE (WHY YOUR I2C DIED)
// ----------------------------------------
// On Arduino Leonardo, I2C uses the SAME electrical nets as:
//   - SDA = D2
//   - SCL = D3
// (even if you plug into the dedicated SDA/SCL header)
//
// Therefore you MUST NOT use D2 or D3 for encoder pins if you are using PCA9685 on I2C.
//
// This firmware:
//  - drives up to 9 motors via two PCA9685 boards (0x40 and 0x41)
//  - supports encoder closed-loop on a subset (default: motors 1 and 2)
//  - uses encoder A on interrupt-capable pins that do NOT conflict with I2C
//      M1: A=D7, B=D4
//      M2: A=D0, B=D5   (D0 is ok; USB Serial is separate. Avoid Serial1 usage.)
//
// Serial robustness + UI fixes:
//  - Uses RISING interrupts (less ISR load/noise sensitivity)
//  - Updates measured RPM even when motor is not enabled (hand-spin shows RPM)
//  - ENC prints ONE motor per line (newline terminated), easy to parse
//
// Commands (newline-terminated):
//   STATUS
//   ENC                 -> prints 9 lines (one per motor): M,id,rpm,count,pwm,tgt,dir,en
//   M3:START:1000:CW
//   M3:SET:900:CCW
//   M3:STOP
//   M3:READ             -> prints 1 line (M,...)
//
// Hardware reminders:
//  - PCA9685 VCC->5V, GND->GND, SDA->SDA, SCL->SCL
//  - Motor driver board MUST be powered separately (PCA9685 cannot drive DC motors directly)
//  - Encoder VCC->5V, GND->GND (common ground with Arduino + motor driver)
//  - Add 0.1uF cap across each brushed DC motor terminals to reduce EMI

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// Two PCA9685 boards at 0x40 and 0x41
Adafruit_PWMServoDriver pwm0(0x40);
Adafruit_PWMServoDriver pwm1(0x41);

// Map 9 motors to (board, IN1 channel, IN2 channel)
struct PwmMap {
  uint8_t board;   // 0 or 1
  uint8_t ch_in1;  // PCA channel for IN1
  uint8_t ch_in2;  // PCA channel for IN2
};

// Motor IDs are 1..9
PwmMap pwmMap[10] = {
  {0, 0,  0},   // index 0 unused
  {0, 0,  1},   // M1 -> board0 ch0,1
  {0, 2,  3},   // M2 -> board0 ch2,3
  {0, 4,  5},   // M3 -> board0 ch4,5
  {0, 6,  7},   // M4 -> board0 ch6,7
  {0, 8,  9},   // M5 -> board0 ch8,9
  {0, 10, 11},  // M6 -> board0 ch10,11
  {0, 12, 13},  // M7 -> board0 ch12,13
  {0, 14, 15},  // M8 -> board0 ch14,15
  {1, 0,  1},   // M9 -> board1 ch0,1
};

// --- Encoder + control configuration ---
// Pololu 12 CPR quadrature encoder.
// We decode using RISING interrupts on channel A only -> 1x decoding (lower ISR load)
static const float ENC_CPR = 12.0f;
static const float GEAR_RATIO = 9.96f;               // per Pololu listing
static const float COUNTS_PER_MOTOR_REV = ENC_CPR * 1.0f; // 1x (RISING only)
static const float COUNTS_PER_OUTPUT_REV = COUNTS_PER_MOTOR_REV * GEAR_RATIO;

static const int MAX_RPM    = 1500;  // output shaft rpm cap
static const int CTRL_DT_MS = 50;    // control loop period

// Encoder pins (IMPORTANT: avoid D2/D3 because they are SDA/SCL on Leonardo)
// Set to 255 for "not present".
uint8_t encA[10] = {255,
  7,   // M1 A -> D7  (interrupt-capable, does not conflict with I2C)
  0,   // M2 A -> D0  (interrupt-capable; avoid Serial1 usage)
  255,255,255,255,255,255,255
};

uint8_t encB[10] = {255,
  4,   // M1 B -> D4
  5,   // M2 B -> D5
  255,255,255,255,255,255,255
};

// --- State tracking ---
volatile long encCount[10] = {0};
long encCountPrev[10]      = {0};

int      targetRpm[10]   = {0};
int      measuredRpm[10] = {0};
uint16_t pwmDuty[10]     = {0};
bool     enabled[10]     = {false};
bool     dirCw[10]       = {true};

// Simple PI controller (per-motor)
float kP[10]    = {0};
float kI[10]    = {0};
float integ[10] = {0};

// Convert 0..4095 duty into PCA output
static inline uint16_t clampU16(int v) {
  if (v < 0) return 0;
  if (v > 4095) return 4095;
  return (uint16_t)v;
}

// Low-level: drive both channels for a motor
void driveMotorRaw(uint8_t id, uint16_t duty, bool cw) {
  if (id < 1 || id > 9) return;

  PwmMap m = pwmMap[id];
  Adafruit_PWMServoDriver &drv = (m.board == 0) ? pwm0 : pwm1;

  if (duty == 0) {
    drv.setPWM(m.ch_in1, 0, 0);
    drv.setPWM(m.ch_in2, 0, 0);
    return;
  }

  if (cw) {
    drv.setPWM(m.ch_in1, 0, duty);
    drv.setPWM(m.ch_in2, 0, 0);
  } else {
    drv.setPWM(m.ch_in1, 0, 0);
    drv.setPWM(m.ch_in2, 0, duty);
  }
}

// RPM <-> counts helpers
int countsToRpm(long dCounts, int dtMs) {
  float cps = (1000.0f * (float)dCounts) / (float)dtMs; // counts/s
  float rps = cps / COUNTS_PER_OUTPUT_REV;
  float rpm = rps * 60.0f;
  if (rpm < 0) rpm = -rpm;
  return (int)(rpm + 0.5f);
}

// Feed-forward: map requested rpm to duty
uint16_t rpmToDutyFF(int rpm) {
  if (rpm <= 0) return 0;
  if (rpm > MAX_RPM) rpm = MAX_RPM;
  float u = (float)rpm / (float)MAX_RPM;  // 0..1
  return clampU16((int)(u * 4095.0f));
}

// --- Encoder ISRs (A-channel only, RISING) ---
void isrEncA1() {
  bool a = digitalRead(encA[1]);
  bool b = digitalRead(encB[1]);
  encCount[1] += (a == b) ? 1 : -1;
}
void isrEncA2() {
  bool a = digitalRead(encA[2]);
  bool b = digitalRead(encB[2]);
  encCount[2] += (a == b) ? 1 : -1;
}

bool hasEncoder(uint8_t id) {
  return (id >= 1 && id <= 9 && encA[id] != 255 && encB[id] != 255);
}

void attachEncoders() {
  // Motor 1
  if (encA[1] != 255) {
    pinMode(encA[1], INPUT_PULLUP);
    pinMode(encB[1], INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(encA[1]), isrEncA1, RISING);
  }
  // Motor 2
  if (encA[2] != 255) {
    pinMode(encA[2], INPUT_PULLUP);
    pinMode(encB[2], INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(encA[2]), isrEncA2, RISING);
  }
}

void setupControllerGains() {
  for (int i = 1; i <= 9; i++) {
    kP[i] = 0.9f;
    kI[i] = 0.15f;
  }
}

void startMotor(uint8_t id, int rpm, bool cw) {
  if (id < 1 || id > 9) return;
  if (rpm < 0) rpm = 0;
  if (rpm > MAX_RPM) rpm = MAX_RPM;

  targetRpm[id] = rpm;
  dirCw[id]     = cw;
  enabled[id]   = true;

  integ[id]   = 0.0f;
  pwmDuty[id] = rpmToDutyFF(rpm);
  driveMotorRaw(id, pwmDuty[id], cw);
}

void stopMotor(uint8_t id) {
  if (id < 1 || id > 9) return;
  enabled[id]   = false;
  targetRpm[id] = 0;
  integ[id]     = 0.0f;
  pwmDuty[id]   = 0;
  driveMotorRaw(id, 0, true);
}

void setMotor(uint8_t id, int rpm, bool cw) {
  if (id < 1 || id > 9) return;
  if (rpm < 0) rpm = 0;
  if (rpm > MAX_RPM) rpm = MAX_RPM;

  targetRpm[id] = rpm;
  dirCw[id]     = cw;

  if (enabled[id]) {
    driveMotorRaw(id, pwmDuty[id], cw);
  }
}

unsigned long lastCtrl = 0;

void controlStep() {
  unsigned long now = millis();
  if (now - lastCtrl < (unsigned long)CTRL_DT_MS) return;
  int dt = (int)(now - lastCtrl);
  lastCtrl = now;

  for (uint8_t id = 1; id <= 9; id++) {

    // Always update encoder-derived RPM (even if EN=0)
    if (hasEncoder(id)) {
      long c;
      noInterrupts(); c = encCount[id]; interrupts();

      long dCounts = c - encCountPrev[id];
      encCountPrev[id] = c;
      measuredRpm[id]  = countsToRpm(dCounts, dt);
    } else {
      measuredRpm[id] = 0;
    }

    // If not enabled, don't drive
    if (!enabled[id]) continue;

    // No encoder -> feedforward only
    if (!hasEncoder(id)) {
      pwmDuty[id] = rpmToDutyFF(targetRpm[id]);
      driveMotorRaw(id, pwmDuty[id], dirCw[id]);
      continue;
    }

    // PI control for encoder-equipped motors
    int tgt = targetRpm[id];
    int err = tgt - measuredRpm[id];

    integ[id] += (float)err * ((float)dt / 1000.0f);
    if (integ[id] > 3000.0f)  integ[id] = 3000.0f;
    if (integ[id] < -3000.0f) integ[id] = -3000.0f;

    float u = (float)rpmToDutyFF(tgt) + kP[id] * (float)err + kI[id] * integ[id];
    pwmDuty[id] = clampU16((int)(u + 0.5f));
    driveMotorRaw(id, pwmDuty[id], dirCw[id]);
  }
}

// CSV telemetry line: M,id,rpm,count,pwm,tgt,dir,en
static void printMotorLine(uint8_t id) {
  long c;
  noInterrupts(); c = encCount[id]; interrupts();

  Serial.print("M,");
  Serial.print(id);
  Serial.print(",");
  Serial.print(measuredRpm[id]);
  Serial.print(",");
  Serial.print(c);
  Serial.print(",");
  Serial.print(pwmDuty[id]);
  Serial.print(",");
  Serial.print(targetRpm[id]);
  Serial.print(",");
  Serial.print(dirCw[id] ? 1 : 0);
  Serial.print(",");
  Serial.print(enabled[id] ? 1 : 0);
}

void setup() {
  Wire.begin();

  pwm0.begin();
  pwm0.setPWMFreq(1600);
  pwm1.begin();
  pwm1.setPWMFreq(1600);

  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 2000) { /* wait */ }

  attachEncoders();
  setupControllerGains();

  lastCtrl = millis();
  Serial.println("READY");
}

void loop() {
  controlStep();

  if (!Serial.available()) return;

  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  if (line == "STATUS") {
    Serial.println("STATUS OK");
    return;
  }

  // ENC prints 9 lines (one per motor) for robust parsing
  if (line == "ENC") {
    for (uint8_t i = 1; i <= 9; i++) {
      printMotorLine(i);
      Serial.println();
    }
    return;
  }

  // parse M<id>:<CMD>:<...>
  int pM     = line.indexOf('M');
  int pColon = line.indexOf(':');
  if (pM != 0 || pColon < 0) {
    Serial.println("ERR BADFMT");
    return;
  }

  int id = line.substring(1, pColon).toInt();
  if (id < 1 || id > 9) {
    Serial.println("ERR ID");
    return;
  }

  String rest = line.substring(pColon + 1);
  int p2      = rest.indexOf(':');
  String cmd  = (p2 >= 0) ? rest.substring(0, p2) : rest;

  if (cmd == "STOP") {
    stopMotor((uint8_t)id);
    Serial.println("OK");
    return;
  }

  if (cmd == "READ") {
    printMotorLine((uint8_t)id);
    Serial.println();
    return;
  }

  // START / SET need rpm and dir
  if (p2 < 0) {
    Serial.println("ERR ARGS");
    return;
  }

  String rest2 = rest.substring(p2 + 1);
  int p3       = rest2.indexOf(':');
  if (p3 < 0) {
    Serial.println("ERR ARGS");
    return;
  }

  int rpm = rest2.substring(0, p3).toInt();
  String dir = rest2.substring(p3 + 1);
  if (rpm < 0) rpm = 0;
  if (rpm > MAX_RPM) rpm = MAX_RPM;

  bool cw = (dir == "CW"); // anything else -> CCW

  if (cmd == "START") {
    startMotor((uint8_t)id, rpm, cw);
    Serial.println("OK");
  } else if (cmd == "SET") {
    setMotor((uint8_t)id, rpm, cw);
    Serial.println("OK");
  } else {
    Serial.println("ERR CMD");
  }
}
