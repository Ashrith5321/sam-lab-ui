// MotorControlNine.ino
// 9-motor PWM + CLOSED-LOOP RPM control (best-effort) for Arduino Leonardo.
//
// IMPORTANT HARDWARE NOTE
// ----------------------
// A Leonardo cannot realistically handle 9 quadrature encoders at high speed
// using external interrupts on all channels.
//
// This firmware therefore:
//  - supports up to 9 motors for PWM output (via two PCA9685 boards)
//  - supports CLOSED-LOOP RPM on a *subset* of motors (default: motors 1 and 2)
//    using interrupts on their encoder A pins
//
// If you need closed-loop for all 9 motors, move to a Teensy / ESP32 / Mega
// or add external QEI/counter chips. The PC-side API stays the same.

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// Two PCA9685 boards at 0x40 and 0x41
Adafruit_PWMServoDriver pwm0 = Adafruit_PWMServoDriver(0x40);
Adafruit_PWMServoDriver pwm1 = Adafruit_PWMServoDriver(0x41);

// Map 9 motors to (board, IN1 channel, IN2 channel)
// Motor IDs are 1..9
struct PwmMap {
  uint8_t board;   // 0 or 1
  uint8_t ch_in1;  // PCA channel for IN1
  uint8_t ch_in2;  // PCA channel for IN2
};

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
// We decode using CHANGE interrupts on channel A only -> 2x decoding.
static const float ENC_CPR = 12.0f;
static const float GEAR_RATIO = 9.96f;              // per Pololu listing
static const float COUNTS_PER_MOTOR_REV = ENC_CPR * 2.0f; // 2x decoding
static const float COUNTS_PER_OUTPUT_REV = COUNTS_PER_MOTOR_REV * GEAR_RATIO;

static const int MAX_RPM = 1500;     // user requested cap (output shaft rpm)
static const int CTRL_DT_MS = 50;    // control loop period

// Encoder pins: only some motors have encoders wired.
// Set to 255 for "not present".
// Leonardo interrupts: 0,1,2,3,7 are interrupt-capable.
// We'll use pins 2 and 3 for motors 1 and 2 by default.
uint8_t encA[10] = {255, 2, 3, 255,255,255,255,255,255,255};
uint8_t encB[10] = {255, 4, 5, 255,255,255,255,255,255,255};

// --- State tracking ---
volatile long encCount[10] = {0};
long encCountPrev[10] = {0};

int targetRpm[10] = {0};
int measuredRpm[10] = {0};
uint16_t pwmDuty[10] = {0};
bool enabled[10] = {false};
bool dirCw[10] = {true};

// Simple PI controller (per-motor)
// Tune these after you get basic readings working.
float kP[10] = {0};
float kI[10] = {0};
float integ[10] = {0};

// Convert 0..4095 duty into PCA output
static inline uint16_t clampU16(int v) {
  if (v < 0) return 0;
  if (v > 4095) return 4095;
  return (uint16_t)v;
}

// Low-level: actually drive both channels for a motor
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
  // output shaft rpm
  float cps = (1000.0f * (float)dCounts) / (float)dtMs;  // counts / s
  float rps = cps / COUNTS_PER_OUTPUT_REV;
  float rpm = rps * 60.0f;
  if (rpm < 0) rpm = -rpm;
  return (int)(rpm + 0.5f);
}

// Feed-forward guess: map requested rpm to duty.
// (You'll still need PI to correct.)
uint16_t rpmToDutyFF(int rpm) {
  if (rpm <= 0) return 0;
  if (rpm > MAX_RPM) rpm = MAX_RPM;
  // assume roughly linear in the region we care about
  float u = (float)rpm / (float)MAX_RPM;
  return clampU16((int)(u * 4095.0f));
}

// --- Encoder ISRs (A-channel only, 2x decode) ---
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

void attachEncoders() {
  // Motor 1
  if (encA[1] != 255) {
    pinMode(encA[1], INPUT_PULLUP);
    pinMode(encB[1], INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(encA[1]), isrEncA1, CHANGE);
  }
  // Motor 2
  if (encA[2] != 255) {
    pinMode(encA[2], INPUT_PULLUP);
    pinMode(encB[2], INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(encA[2]), isrEncA2, CHANGE);
  }
}

void startMotor(uint8_t id, int rpm, bool cw) {
  if (id < 1 || id > 9) return;
  if (rpm < 0) rpm = 0;
  if (rpm > MAX_RPM) rpm = MAX_RPM;

  targetRpm[id] = rpm;
  dirCw[id] = cw;
  enabled[id] = true;

  // reset controller state
  integ[id] = 0.0f;
  pwmDuty[id] = rpmToDutyFF(rpm);
  driveMotorRaw(id, pwmDuty[id], cw);
}

void stopMotor(uint8_t id) {
  if (id < 1 || id > 9) return;
  enabled[id] = false;
  targetRpm[id] = 0;
  integ[id] = 0.0f;
  pwmDuty[id] = 0;
  driveMotorRaw(id, 0, true);
}

void setMotor(uint8_t id, int rpm, bool cw) {
  if (id < 1 || id > 9) return;
  if (rpm < 0) rpm = 0;
  if (rpm > MAX_RPM) rpm = MAX_RPM;

  targetRpm[id] = rpm;
  dirCw[id] = cw;
  if (enabled[id]) {
    // keep current duty; PI will steer it
    driveMotorRaw(id, pwmDuty[id], cw);
  }
}

bool hasEncoder(uint8_t id) {
  return (id >= 1 && id <= 9 && encA[id] != 255 && encB[id] != 255);
}

void setupControllerGains() {
  // Start conservative; tune per motor.
  // Units: rpm error -> duty (0..4095)
  for (int i = 1; i <= 9; i++) {
    kP[i] = 0.9f;
    kI[i] = 0.15f;
  }
}

unsigned long lastCtrl = 0;

void controlStep() {
  unsigned long now = millis();
  if (now - lastCtrl < (unsigned long)CTRL_DT_MS) return;
  int dt = (int)(now - lastCtrl);
  lastCtrl = now;

  for (uint8_t id = 1; id <= 9; id++) {
    if (!enabled[id]) continue;

    // If no encoder wired, we cannot close the loop: just do feedforward.
    if (!hasEncoder(id)) {
      pwmDuty[id] = rpmToDutyFF(targetRpm[id]);
      driveMotorRaw(id, pwmDuty[id], dirCw[id]);
      continue;
    }

    long c;
    noInterrupts();
    c = encCount[id];
    interrupts();

    long dCounts = c - encCountPrev[id];
    encCountPrev[id] = c;
    measuredRpm[id] = countsToRpm(dCounts, dt);

    int tgt = targetRpm[id];
    int err = tgt - measuredRpm[id];

    // PI
    integ[id] += (float)err * ((float)dt / 1000.0f);
    // anti-windup
    if (integ[id] > 3000.0f) integ[id] = 3000.0f;
    if (integ[id] < -3000.0f) integ[id] = -3000.0f;

    float u = (float)rpmToDutyFF(tgt) + kP[id] * (float)err + kI[id] * integ[id];
    pwmDuty[id] = clampU16((int)(u + 0.5f));
    driveMotorRaw(id, pwmDuty[id], dirCw[id]);
  }
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

// Command format examples:
// STATUS
// ENC                  -> one-line all-motor telemetry
// M3:START:1000:CW      (rpm is OUTPUT shaft rpm)
// M3:SET:900:CCW
// M3:STOP
// M3:READ               -> one line for this motor

static void printMotorLine(uint8_t id) {
  long c;
  noInterrupts();
  c = encCount[id];
  interrupts();
  Serial.print("M"); Serial.print(id);
  Serial.print(":RPM:"); Serial.print(measuredRpm[id]);
  Serial.print(":COUNT:"); Serial.print(c);
  Serial.print(":PWM:"); Serial.print(pwmDuty[id]);
  Serial.print(":TGT:"); Serial.print(targetRpm[id]);
  Serial.print(":DIR:"); Serial.print(dirCw[id] ? "CW" : "CCW");
  Serial.print(":EN:"); Serial.print(enabled[id] ? 1 : 0);
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

  if (line == "ENC") {
    Serial.print("ENC ");
    for (uint8_t i = 1; i <= 9; i++) {
      printMotorLine(i);
      if (i != 9) Serial.print(";");
    }
    Serial.println();
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

  int    rpm = rest2.substring(0, p3).toInt();
  String dir = rest2.substring(p3 + 1);
  if (rpm < 0) rpm = 0;
  if (rpm > MAX_RPM) rpm = MAX_RPM;
  bool cw = (dir == "CW");

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
