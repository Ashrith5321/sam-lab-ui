// Minimal encoder pin test for Teensy 4.1
// Upload this, connect ONE encoder to the test pin pair,
// spin the motor by hand, and watch Serial Monitor.
// Change TEST_PIN_A and TEST_PIN_B to test different pins.

#define TEST_PIN_A  6   // <-- CHANGE THIS to test different pins
#define TEST_PIN_B  7   // <-- CHANGE THIS to test different pins

volatile long count = 0;

void encoderISR() {
  bool a = digitalRead(TEST_PIN_A);
  bool b = digitalRead(TEST_PIN_B);
  if (a == b) count++;
  else count--;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  pinMode(TEST_PIN_A, INPUT_PULLUP);
  pinMode(TEST_PIN_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TEST_PIN_A), encoderISR, CHANGE);

  Serial.println("=== PIN TEST ===");
  Serial.print("Testing encoder on pins: A=");
  Serial.print(TEST_PIN_A);
  Serial.print(" B=");
  Serial.println(TEST_PIN_B);
  Serial.print("Pin A reads: ");
  Serial.println(digitalRead(TEST_PIN_A));
  Serial.print("Pin B reads: ");
  Serial.println(digitalRead(TEST_PIN_B));
  Serial.println("Spin the motor and watch the count change...");
}

void loop() {
  static long lastCount = 0;
  long c = count;
  if (c != lastCount) {
    Serial.print("Count: ");
    Serial.print(c);
    Serial.print("  (delta: ");
    Serial.print(c - lastCount);
    Serial.println(")");
    lastCount = c;
  }
  delay(100);
}
