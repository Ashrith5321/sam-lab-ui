#pragma once
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

class SerialPort {
public:
  using LineCallback = std::function<void(const std::string&)>;

  SerialPort(const std::string& path, int baud = 115200);
  ~SerialPort();

  bool open();
  void close();
  bool isOpen() const { return fd_ >= 0; }

  bool writeLine(const std::string& line);
  void setLineCallback(LineCallback cb);

private:
  void readerLoop();

  std::string path_;
  int baud_;
  int fd_ = -1;
  std::thread reader_;
  std::atomic<bool> running_{false};
  std::string buf_;
  std::mutex cb_mtx_;
  LineCallback cb_;
};
