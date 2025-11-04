#pragma once
#include <string>
#include <mutex>
#include <vector>

// Minimal POSIX serial wrapper (Linux)
class SerialPort {
public:
    SerialPort();
    ~SerialPort();

    bool open(const std::string &device, int baud = 115200);
    void close();
    bool isOpen() const { return fd_ >= 0; }

    // write a full line and append \n

    bool writeLine(const std::string &line);

    // blocking read of one line (up to max_ms timeout if >0). Returns true if got a line.
    bool readLine(std::string &out, int max_ms = 0);

private:
    int fd_;
    std::mutex mtx_;
    bool configure(int baud);
};
