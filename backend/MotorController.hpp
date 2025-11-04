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

    bool start(int id, int speedPercent, Direction dir);
    bool stop(int id);
    bool set(int id, int speedPercent, Direction dir);

private:
    SerialPort sp_;
    bool sendLine(const std::string &line, int expectAckMs=100);
};
