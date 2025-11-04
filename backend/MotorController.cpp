#include "MotorController.hpp"
#include <iostream>
#include <sstream>

bool MotorController::connect(const std::string &device, int baud){
    if (!sp_.open(device, baud)) return false;
    std::cerr << "Serial open at " << device << " @" << baud << "\n";

    // Try to catch READY for a bit
    std::string line;
    const int ms = 3000;
    if (sp_.readLine(line, ms)) {
        std::cerr << "[SERIAL←] " << line << "\n";
    } else {
        std::cerr << "[SERIAL←] (no READY in " << ms << " ms)\n";
    }
    return true;
}

std::optional<std::string> MotorController::status(){
    sp_.writeLine("STATUS");
    std::string line; if (sp_.readLine(line, 500)) return line; else return std::nullopt;
}

bool MotorController::start(int id, int speedPercent, Direction dir){
    std::ostringstream ss; ss << "M"<<id<<":START:"<<speedPercent<<":"<<(dir==Direction::CW?"CW":"CCW");
    return sendLine(ss.str());
}

bool MotorController::stop(int id){
    std::ostringstream ss; ss << "M"<<id<<":STOP";
    return sendLine(ss.str());
}

bool MotorController::set(int id, int speedPercent, Direction dir){
    std::ostringstream ss; ss << "M"<<id<<":SET:"<<speedPercent<<":"<<(dir==Direction::CW?"CW":"CCW");
    return sendLine(ss.str());
}

bool MotorController::sendLine(const std::string &line, int expectAckMs){
    std::cerr << "[SERIAL→] " << line << "\n";
    if (!sp_.writeLine(line)) return false;

    // Be generous with time; Leonardo may reset or be busy.
    int timeout = std::max(expectAckMs, 800);   // was 100

    std::string resp;
    if (sp_.readLine(resp, timeout)) {
        std::cerr << "[SERIAL←] " << resp << "\n";
        return resp=="OK" || resp=="STATUS OK";
    }
    // No reply isn't fatal; firmware might be busy
    std::cerr << "[SERIAL←] (no-reply)\n";
    return true;
}