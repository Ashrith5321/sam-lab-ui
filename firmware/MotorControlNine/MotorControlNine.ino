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

// State tracking
uint8_t speedPct[10] = {0};
char    dirStr[10]   = {0};   // 'C' for CW, 'A' for CCW
bool    enabled[10]  = {false};

// Convert 0..100% to PCA9685 12-bit (0..4095)
uint16_t pctToPwm(uint8_t pct) {
  if (pct == 0) return 0;
  if (pct > 100) pct = 100;
  return (uint16_t)((pct * 4095UL) / 100UL);
}

// Low-level: actually drive both channels for a motor
void driveMotorRaw(uint8_t id, uint16_t duty, bool cw) {
  if (id < 1 || id > 9) return;

  PwmMap m = pwmMap[id];
  Adafruit_PWMServoDriver &drv = (m.board == 0) ? pwm0 : pwm1;

  uint16_t d = duty;

  if (d == 0) {
    // Motor off: both low
    drv.setPWM(m.ch_in1, 0, 0);
    drv.setPWM(m.ch_in2, 0, 0);
    return;
  }

  if (cw) {
    // CW: IN1 = PWM, IN2 = 0
    drv.setPWM(m.ch_in1, 0, d);
    drv.setPWM(m.ch_in2, 0, 0);
  } else {
    // CCW: IN1 = 0, IN2 = PWM
    drv.setPWM(m.ch_in1, 0, 0);
    drv.setPWM(m.ch_in2, 0, d);
  }
}

// High-level helpers

void startMotor(uint8_t id, uint8_t sp, bool cw) {
  if (id < 1 || id > 9) return;
  speedPct[id] = sp;
  enabled[id]  = true;
  dirStr[id]   = cw ? 'C' : 'A';
  driveMotorRaw(id, pctToPwm(sp), cw);
}

void stopMotor(uint8_t id) {
  if (id < 1 || id > 9) return;
  enabled[id] = false;
  driveMotorRaw(id, 0, true);  // direction doesn't matter when duty=0
}

void setMotor(uint8_t id, uint8_t sp, bool cw) {
  if (id < 1 || id > 9) return;
  speedPct[id] = sp;
  dirStr[id]   = cw ? 'C' : 'A';
  if (enabled[id]) {
    driveMotorRaw(id, pctToPwm(sp), cw);
  }
}

void setup() {
  Wire.begin();

  pwm0.begin();
  pwm0.setPWMFreq(1600);

  pwm1.begin();
  pwm1.setPWMFreq(1600);

  Serial.begin(115200);

  // Leonardo: wait up to 2s for USB serial, but don't block forever
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 2000) { /* wait */ }

  Serial.println("READY");
}

// Command format examples:
// STATUS
// M3:START:80:CW
// M3:STOP
// M7:SET:55:CCW

void loop() {
  if (!Serial.available()) {
    return;
  }

  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  if (line == "STATUS") {
    Serial.println("STATUS OK");
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
    stopMotor(id);
    Serial.println("OK");
    return;
  }

  // START / SET need speed and dir
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

  int    sp  = rest2.substring(0, p3).toInt();
  String dir = rest2.substring(p3 + 1);
  if (sp < 0) sp = 0;
  if (sp > 100) sp = 100;
  bool cw = (dir == "CW");   // anything else is treated as CCW

  if (cmd == "START") {
    startMotor(id, (uint8_t)sp, cw);
    Serial.println("OK");
  } else if (cmd == "SET") {
    setMotor(id, (uint8_t)sp, cw);
    Serial.println("OK");
  } else {
    Serial.println("ERR CMD");
  }
}
