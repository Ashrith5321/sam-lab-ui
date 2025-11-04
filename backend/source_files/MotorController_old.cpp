// backend/MotorController.cpp
#include "MotorController.hpp"
#include <iostream>
#include <thread>
#include <chrono>

MotorController::MotorController(const std::string& serial_path, int baud)
  : serial_(serial_path, baud) {}

bool MotorController::start() {
  if (started_.exchange(true)) return true;
  serial_.setLineCallback([this](const std::string& line){ onLine(line); });
  bool ok = serial_.open();
  // small delay helps Leonardo after open
  std::this_thread::sleep_for(std::chrono::milliseconds(400));
  if (ok) serial_.writeLine("STATUS");
  return ok;
}

void MotorController::stop() {
  if (!started_.exchange(false)) return;
  serial_.close();
}

bool MotorController::turnOn()  {
  std::cerr << "[SERIAL→] ON\n";
  bool ok = serial_.writeLine("ON");
  if (!ok) std::cerr << "[SERIAL] write ON failed\n";
  return ok;
}
bool MotorController::turnOff() {
  std::cerr << "[SERIAL→] OFF\n";
  bool ok = serial_.writeLine("OFF");
  if (!ok) std::cerr << "[SERIAL] write OFF failed\n";
  return ok;
}
bool MotorController::requestStatus() { return serial_.writeLine("STATUS"); }

std::string MotorController::state() const {
  int s = stateFlag_.load();
  if (s == 1) return "on";
  if (s == 2) return "off";
  return "unknown";
}

void MotorController::onLine(const std::string& line) {
  std::cerr << "[SERIAL←] " << line << "\n";
  if (line == "OK ON" || line == "STATUS ON")  stateFlag_.store(1);
  else if (line == "OK OFF" || line == "STATUS OFF") stateFlag_.store(2);
}
