// firmware/MotorControlOne.ino
const uint8_t PIN_IN1 = 5;  // PWM-capable
const uint8_t PIN_IN2 = 4;
const uint8_t ON_SPEED = 255;

enum MotorState { STATE_OFF = 0, STATE_ON = 1 };
MotorState currentState = STATE_OFF;
String rx;

void setMotorOff() {
  digitalWrite(PIN_IN2, LOW);
  analogWrite(PIN_IN1, 0);
  currentState = STATE_OFF;
}

void setMotorOn() {
  digitalWrite(PIN_IN2, LOW);
  analogWrite(PIN_IN1, ON_SPEED);
  currentState = STATE_ON;
}

void setup() {
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  setMotorOff();

  Serial.begin(115200);
  while (!Serial) { /* Leonardo: wait for USB serial */ }
  delay(100);
  Serial.println("READY");
}
➜  ashed-Yoga-Pro-9-16IMH9 at /home/ashed/Documents/one-motor-cpp SERIAL_PORT=/dev/serial/by-id/usb-Arduino_LLC_Arduino_Leonardo-if00 \
PORT=5173 \
./build/one_motor

[SERIAL←] READY
Serial open at /dev/serial/by-id/usb-Arduino_LLC_Arduino_Leonardo-if00 @115200
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
HTTP serving ./public on http://127.0.0.1:5173
HTTP listening on http://127.0.0.1:5173
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
[HTTP] GET /
[HTTP] Content-Length=0, body=''
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
[HTTP] POST /motor
[HTTP] Content-Length=14, body='{"state":"on"}'
[HTTP] /motor raw body (14): {"state":"on"}
[SERIAL→] ON
[HTTP] command sent: ON, writeOk=true
[SERIAL←] ON
[SERIAL←] OK ON
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS ON
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS ON
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS ON
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS ON
[HTTP] POST /motor
[HTTP] Content-Length=15, body='{"state":"off"}'
[HTTP] /motor raw body (15): {"state":"off"}
[SERIAL→] OFF
[HTTP] command sent: OFF, writeOk=true
[SERIAL←] OFF
[SERIAL←] OK OFF
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
[HTTP] POST /motor
[HTTP] Content-Length=14, body='{"state":"on"}'
[HTTP] /motor raw body (14): {"state":"on"}
[SERIAL→] ON
[HTTP] command sent: ON, writeOk=true
[SERIAL←] ON
[SERIAL←] OK ON
[HTTP] POST /motor
[HTTP] Content-Length=15, body='{"state":"off"}'
[HTTP] /motor raw body (15): {"state":"off"}
[SERIAL→] OFF
[HTTP] command sent: OFF, writeOk=true
[SERIAL←] OFF
[SERIAL←] OK OFF
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS OFF
[HTTP] POST /motor
[HTTP] Content-Length=14, body='{"state":"on"}'
[HTTP] /motor raw body (14): {"state":"on"}
[SERIAL→] ON
[HTTP] command sent: ON, writeOk=true
[SERIAL←] ON
[SERIAL←] OK ON
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS ON
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS ON
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS ON
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS ON
[HTTP] GET /status
[HTTP] Content-Length=0, body=''
[SERIAL←] STATUS
[SERIAL←] STATUS ON
void loop() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    // Echo typed characters so you can see them in 'screen'
    Serial.write(c);

    // Treat either CR or LF as end-of-line
    if (c == '\r' || c == '\n') {
      if (rx.length() > 0) {     // only handle non-empty lines
        handleCommand(rx);
        rx = "";
      }
      // ignore multiple CR/LF in a row
    } else {
      rx += c;
      if (rx.length() > 64) rx = "";  // simple overflow guard
    }
  }
}
void handleCommand(String cmd) {
  cmd.trim(); cmd.toUpperCase();
  if (cmd == "ON")      { setMotorOn();  Serial.println("OK ON"); }
  else if (cmd == "OFF"){ setMotorOff(); Serial.println("OK OFF"); }
  else if (cmd == "STATUS") {
    Serial.println(currentState == STATE_ON ? "STATUS ON" : "STATUS OFF");
  } else {
    Serial.println("ERR");
  }
}
