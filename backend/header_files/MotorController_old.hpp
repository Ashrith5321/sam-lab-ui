#pragma once
#include "SerialPort.hpp"
#include <atomic>
#include <string>
#include <mutex>

class MotorController {
public:
  explicit MotorController(const std::string& serial_path, int baud = 115200);

  bool start();
  void stop();

  // Fire-and-forget commands; returns whether write succeeded
  bool turnOn();
  bool turnOff();

  // Returns cached state ("on"/"off"/"unknown")
  std::string state() const;

  // Ask Arduino for status (non-blocking write; state updates when reply arrives)
  bool requestStatus();

private:
  void onLine(const std::string& line);

  SerialPort serial_;
  std::atomic<bool> started_{false};
  std::atomic<int> stateFlag_{0}; // 0 unknown, 1 on, 2 off
};
