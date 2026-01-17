#pragma once
#include <string>
#include <optional>
#include "SerialPort.hpp"

enum class Direction { CW, CCW };

class MotorController {
public:
    bool connect(const std::string &device, int baud = 115200);

    // returns Arduino one-line reply if available
    std::optional<std::string> status();

    // Closed-loop speed control (RPM at OUTPUT SHAFT, after gearbox)
    bool start(int id, int targetRpm, Direction dir);
    bool stop(int id);
    bool set(int id, int targetRpm, Direction dir);

    // One-shot readback of speed/encoder for a motor.
    // Firmware replies with a single line like:
    //  M1:RPM:1234:COUNT:567: PWM:2048:TGT:1500
    std::optional<std::string> read(int id);

    // Read all motors at once (best-effort). Firmware replies one line.
    //  ENC M1=...;M2=...;...
    std::optional<std::string> readAll();

private:
    SerialPort sp_;
    bool sendLine(const std::string &line, int expectAckMs=100);
};