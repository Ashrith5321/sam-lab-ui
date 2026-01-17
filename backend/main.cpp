#include <cstdlib>
#include <iostream>
#include <regex>
#include <string>
#include <algorithm>  // for std::min/std::max
#include <cctype>
#include <sstream>
#include "HttpServer.hpp"
#include "MotorController.hpp"

int main() {
    const char* serialEnv = std::getenv("SERIAL_PORT");
    std::string serial = serialEnv ? std::string(serialEnv) : std::string();
    if (serial.empty()) {
        std::cerr << "Set SERIAL_PORT to your Arduino device (e.g., /dev/serial/by-id/...)\n";
        return 1;
    }

    const char* portEnv = std::getenv("PORT");
    int port = portEnv ? std::atoi(portEnv) : 5173;

    const char* staticEnv = std::getenv("STATIC_DIR");
    std::string staticDir = staticEnv ? std::string(staticEnv) : std::string("./public");

    MotorController mc;
    if (!mc.connect(serial)) return 2;

    HttpServer http;
auto handler = [&mc](const std::string& method,
                     const std::string& path,
                     const std::string& body,
                     int& status,
                     std::string& ctype) -> std::string {
    (void)body;
    ctype = "application/json";

    // Debug: see exactly what we got
    std::cerr << "DEBUG handler: method=" << method
              << " path='" << path << "'" << std::endl;

    // 1) /api/status
    if (path == "/api/status") {
        auto s = mc.status();
        status = 200;
        return std::string("{\"status\":\"") + (s ? *s : "NO-REPLY") + "\"}";
    }

    auto jsonEscape = [](const std::string& in) {
        std::string out;
        out.reserve(in.size() + 8);
        for (char c : in) {
            switch (c) {
                case '\\': out += "\\\\"; break;
                case '"':  out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:    out += c; break;
            }
        }
        return out;
    };

    // 2) /api/encoders  (poll all motors)
    if (path == "/api/encoders") {
        auto line = mc.readAll();
        status = 200;
        // pass through as a raw string (still JSON)
        return std::string("{\"line\":\"") + jsonEscape(line ? *line : "") + "\"}";
    }

    // 3) /api/motor/{id}/read
    {
        std::smatch mr;
        std::regex rRead(R"(^/api/motor/(\d+)/read$)");
        if (std::regex_match(path, mr, rRead)) {
            int id = std::stoi(mr[1]);
            if (id < 1 || id > 9) {
                status = 400;
                return "{\"error\":\"invalid id; expected 1..9\"}";
            }
            auto line = mc.read(id);
            status = 200;
            return std::string("{\"line\":\"") + jsonEscape(line ? *line : "") + "\"}";
        }
    }

    // 4) /api/motor/{id}/{start|stop|set}?rpm=..&dir=..
    std::smatch m;
    std::regex rSet(R"(^/api/motor/(\d+)/(start|stop|set)(?:\?([^#]*))?$)");

    if (std::regex_match(path, m, rSet)) {
        int id = std::stoi(m[1]);
        std::string cmd = m[2];
        std::string qs  = (m.size() > 3 && m[3].matched) ? m[3].str() : "";

        std::cerr << "DEBUG match: id=" << id
                  << " cmd=" << cmd
                  << " qs='" << qs << "'" << std::endl;

        if (id < 1 || id > 9) {
            status = 400;
            return "{\"error\":\"invalid id; expected 1..9\"}";
        }

        int rpm = 0;
        std::string dirStr = "CW";   // default direction if none specified

        // --- Simple query string parser: speed=..&dir=.. ---
        std::istringstream qss(qs);
        std::string pair;
        while (std::getline(qss, pair, '&')) {
            if (pair.empty()) continue;
            auto eqPos = pair.find('=');
            if (eqPos == std::string::npos) continue;

            std::string k = pair.substr(0, eqPos);
            std::string v = pair.substr(eqPos + 1);

            std::cerr << "DEBUG kv: '" << k << "'='" << v << "'" << std::endl;

            if (k == "rpm") {
                // We cap in firmware too, but clamp here for nicer behavior.
                rpm = std::max(0, std::min(1500, std::atoi(v.c_str())));
            } else if (k == "dir") {
                dirStr = v;
            }
        }

        // normalize dir string
        auto trim = [](std::string &s) {
            auto is_space = [](unsigned char c){ return std::isspace(c); };
            while (!s.empty() && is_space(s.front())) s.erase(s.begin());
            while (!s.empty() && is_space(s.back())) s.pop_back();
        };

        trim(dirStr);
        for (char &c : dirStr) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }

        Direction d = Direction::CW;
        if (dirStr.rfind("CCW", 0) == 0) {
            d = Direction::CCW;
        }

        std::cerr << "DEBUG parsed: rpm=" << rpm
                  << " dirStr='" << dirStr << "' -> "
                  << (d == Direction::CCW ? "CCW" : "CW")
                  << std::endl;


        bool ok = false;
        if (cmd == "start") {
            ok = mc.start(id, rpm, d);
        } else if (cmd == "stop") {
            ok = mc.stop(id);
        } else if (cmd == "set") {
            ok = mc.set(id, rpm, d);
        }

        status = ok ? 200 : 500;
        return std::string("{\"ok\":") + (ok ? "true" : "false") + "}";
    }

    status = 404;
    return "{\"error\":\"not found\"}";
};


    if (!http.start(static_cast<unsigned short>(port), staticDir, handler)) {
        std::cerr << "Failed to start HTTP server\n";
        return 3;
    }

    std::cerr << "HTTP serving " << staticDir << " on http://127.0.0.1:" << port << "\n";

    // block forever
    std::string dummy;
    std::getline(std::cin, dummy);
    http.stop();
    return 0;
}