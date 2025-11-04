#include "SerialPort.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <cstring>
#include <iostream>

static speed_t baud_to_speed(int baud) {
  switch (baud) {
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
    default: return B115200;
  }
}

SerialPort::SerialPort(const std::string& path, int baud)
  : path_(path), baud_(baud) {}

SerialPort::~SerialPort() {
  close();
}

bool SerialPort::open() {
  fd_ = ::open(path_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd_ < 0) {
    std::perror("open serial");
    return false;
  }

  termios tty{};
  if (tcgetattr(fd_, &tty) != 0) {
    std::perror("tcgetattr");
    ::close(fd_); fd_ = -1; return false;
  }

  cfmakeraw(&tty);
  tty.c_cflag |= (CLOCAL | CREAD);
  tty.c_cflag &= ~CSTOPB;        // 1 stop
  tty.c_cflag &= ~CRTSCTS;       // no HW flow
  tty.c_cflag &= ~PARENB;        // no parity
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;            // 8 data bits

  speed_t spd = baud_to_speed(baud_);
  cfsetispeed(&tty, spd);
  cfsetospeed(&tty, spd);

  tty.c_cc[VMIN]  = 0;  // non-block
  tty.c_cc[VTIME] = 5;  // 0.5s read timeout

  if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
    std::perror("tcsetattr");
    ::close(fd_); fd_ = -1; return false;
  }

  // flush
  tcflush(fd_, TCIOFLUSH);

  running_ = true;
  reader_ = std::thread(&SerialPort::readerLoop, this);
  return true;
}

void SerialPort::close() {
  running_ = false;
  if (reader_.joinable()) reader_.join();
  if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

bool SerialPort::writeLine(const std::string& line) {
  if (fd_ < 0) return false;
  std::string out = line + "\n";
  ssize_t n = ::write(fd_, out.data(), out.size());
  return n == (ssize_t)out.size();
}

void SerialPort::setLineCallback(LineCallback cb) {
  std::lock_guard<std::mutex> lk(cb_mtx_);
  cb_ = std::move(cb);
}

void SerialPort::readerLoop() {
  char tmp[256];
  while (running_) {
    if (fd_ < 0) break;
    ssize_t n = ::read(fd_, tmp, sizeof(tmp));
    if (n > 0) {
      buf_.append(tmp, tmp + n);
      size_t pos;
      for (;;) {
        pos = buf_.find('\n');
        if (pos == std::string::npos) break;
        std::string line = buf_.substr(0, pos);
        // trim CR
        if (!line.empty() && line.back() == '\r') line.pop_back();

        SerialPort::LineCallback cb_copy;
        {
          std::lock_guard<std::mutex> lk(cb_mtx_);
          cb_copy = cb_;
        }
        if (cb_copy) cb_copy(line);

        buf_.erase(0, pos + 1);
      }
    } else {
      // small sleep to avoid busy-loop
      usleep(1000 * 5);
    }
  }
}
