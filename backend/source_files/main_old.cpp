// backend/main.cpp
#include "HttpServer.hpp"
#include "MotorController.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>
#include <chrono>
#include <unistd.h>

static std::string json_escape(const std::string& s) {
  std::string o; o.reserve(s.size()+8);
  for (char c : s) {
    switch (c) { case '"': o += "\\\""; break; case '\\': o += "\\\\"; break; case '\n': o += "\\n"; break; default: o += c; }
  }
  return o;
}

int main(int, char**) {
  const char* env_port = std::getenv("PORT");
  const int http_port = env_port ? std::atoi(env_port) : 5173;

  const char* env_spath = std::getenv("SERIAL_PORT");
  std::string serial_path = env_spath ? env_spath : "/dev/ttyACM0";

  std::string public_index = (std::filesystem::current_path() / "public" / "index.html").string();

  MotorController motor(serial_path, 115200);
  if (!motor.start()) {
    std::cerr << "Failed to open serial at " << serial_path << "\n";
  } else {
    std::cerr << "Serial open at " << serial_path << " @115200\n";
    // Give Leonardo a moment after open, then prime a status
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    motor.requestStatus();
  }

  HttpServer::Handlers handlers;
  handlers.getIndexPath = [public_index]() { return public_index; };

  handlers.getStatusJson = [&motor, serial_path]() {
    motor.requestStatus(); // non-blocking; updates cache when reply arrives
    std::string st = motor.state();
    return std::string("{\"ok\":true,\"state\":\"") + st +
           "\",\"port\":\"" + json_escape(serial_path) + "\"}";
  };

  handlers.postMotorJson = [&motor](const std::string& body) {
    std::cerr << "[HTTP] /motor raw body (" << body.size() << "): " << body << "\n";

    // normalize
    std::string b = body;
    for (auto& c : b) c = std::tolower(static_cast<unsigned char>(c));
    auto has = [&](const std::string& s){ return b.find(s) != std::string::npos; };

    bool setOn=false, setOff=false;
    // JSON exact
    if (has("\"state\":\"on\""))  setOn = true;
    if (has("\"state\":\"off\"")) setOff = true;
    // tokens / raw
    if (has("\"on\"")  || b == "on")   setOn = true;
    if (has("\"off\"") || b == "off")  setOff = true;
    // form
    if (has("state=on"))  setOn = true;
    if (has("state=off")) setOff = true;

    if (setOn && setOff) return std::string("{\"ok\":false,\"error\":\"ambiguous: both on and off present\"}");
    if (!(setOn || setOff)) return std::string("{\"ok\":false,\"error\":\"state must be 'on' or 'off'\"}");

    bool writeOk = setOn ? motor.turnOn() : motor.turnOff();
    std::cerr << "[HTTP] command sent: " << (setOn ? "ON" : "OFF") << ", writeOk=" << (writeOk ? "true" : "false") << "\n";
    const char* st = setOn ? "on" : "off";
    return std::string("{\"ok\":") + (writeOk ? "true" : "false") + ",\"state\":\"" + st + "\"}";
  };

  HttpServer server("127.0.0.1", http_port, handlers);
  server.start();

  std::cerr << "HTTP serving ./public on http://127.0.0.1:" << http_port << "\n";
  for(;;) pause();

  server.stop();
  motor.stop();
  return 0;
}
