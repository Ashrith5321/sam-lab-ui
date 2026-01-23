#include "SerialPort.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>
#include <cstring>
#include <iostream>
#include <sys/ioctl.h>
#include <chrono>



static speed_t baudToFlag(int baud){
    switch(baud){
        case 9600: return B9600; case 19200: return B19200; case 38400: return B38400;
        case 57600: return B57600; case 115200: return B115200; default: return B115200; }
}

SerialPort::SerialPort(): fd_(-1) {}
SerialPort::~SerialPort(){ close(); }

bool SerialPort::open(const std::string &device, int baud){
    std::lock_guard<std::mutex> lk(mtx_);
    fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) { perror("open serial"); return false; }
    if (!configure(baud)) { close(); return false; }

    // set blocking
    int flags = fcntl(fd_, F_GETFL, 0);
    fcntl(fd_, F_SETFL, flags & ~O_NONBLOCK);

    // Assert DTR/RTS so ATmega32U4 CDC is fully "open"
    int mflags = 0;
    if (ioctl(fd_, TIOCMGET, &mflags) == 0) {
        mflags |= TIOCM_DTR | TIOCM_RTS;
        ioctl(fd_, TIOCMSET, &mflags);
    }
    return true;
}

void SerialPort::close(){
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

bool SerialPort::configure(int baud){
    struct termios tio{};
    if (tcgetattr(fd_, &tio) < 0) return false;
    cfmakeraw(&tio);
    tio.c_cflag |= CLOCAL | CREAD;
    speed_t sp = baudToFlag(baud);
    cfsetispeed(&tio, sp);
    cfsetospeed(&tio, sp);
    tio.c_cc[VMIN] = 0;  // read returns immediately
    tio.c_cc[VTIME] = 1; // 100ms read timeout
    return tcsetattr(fd_, TCSANOW, &tio) == 0;
}

bool SerialPort::writeLine(const std::string &line){
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ < 0) return false;
    std::string out = line + "\n";
    ssize_t n = ::write(fd_, out.c_str(), out.size());
    return n == (ssize_t)out.size();
}

bool SerialPort::readLine(std::string &out, int max_ms){
    out.clear();
    if (fd_ < 0) return false;

    std::string buf;
    char ch;

    auto start = std::chrono::steady_clock::now();

    while (true){
        int remaining = max_ms;
        if (max_ms > 0) {
            auto now = std::chrono::steady_clock::now();
            int elapsed = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            remaining = max_ms - elapsed;
            if (remaining <= 0) return false;
        }

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd_, &rfds);

        struct timeval tv;
        struct timeval* ptv = nullptr;
        if (max_ms > 0) {
            tv = { remaining/1000, (remaining%1000)*1000 };
            ptv = &tv;
        } else {
            // No timeout requested: still avoid hard blocking forever
            tv = { 0, 200000 }; // 200ms tick
            ptv = &tv;
        }

        int rv = select(fd_+1, &rfds, nullptr, nullptr, ptv);
        if (rv < 0) return false;
        if (rv == 0) {
            if (max_ms > 0) continue;
            // if max_ms<=0, just keep waiting in ticks
            continue;
        }

        ssize_t n = ::read(fd_, &ch, 1);
        if (n == 1){
            if (ch == '\n') { out = buf; return true; }
            if (ch != '\r') buf.push_back(ch);
        } else if (n == 0) {
            // nothing read; loop again
            continue;
        } else {
            // n < 0: error
            return false;
        }
    }
}