#include <cstdlib>
#include <iostream>
#include <regex>
#include <string>
#include <algorithm>  // for std::min/std::max

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
        (void)body; // unused
        ctype = "application/json";

        // Allow simple CORS if you ever hit from another origin
        // if (method == "OPTIONS") { status = 200; return "{}"; }

        std::smatch m;
        std::regex rSet(R"(^/api/motor/(\d+)/(start|stop|set)(?:\?([^#]*))?$)");

        if (path == "/api/status") {
            auto s = mc.status();
            status = 200;
            return std::string("{\"status\":\"") + (s ? *s : "NO-REPLY") + "\"}";
        }

        if (std::regex_match(path, m, rSet)) {
            int id = std::stoi(m[1]);
            if (id < 1 || id > 9) {
                status = 400;
                return "{\"error\":\"invalid id; expected 1..9\"}";
            }

            std::string cmd = m[2];
            std::string qs = (m.size() > 3 && m[3].matched) ? m[3].str() : "";


            int speed = 0;
            std::string dir = "CW";

            // parse query string speed= & dir=
            std::regex rKV(R"((^|&)([^=]+)=([^&]+))");
            std::smatch kv;
            std::string rest = qs;
            while (std::regex_search(rest, kv, rKV)) {
                std::string k = kv[2];
                std::string v = kv[3];
                if (k == "speed") {
                    speed = std::max(0, std::min(100, std::atoi(v.c_str())));
                } else if (k == "dir") {
                    dir = v;
                }
                rest = kv.suffix();
            }

            bool ok = false;
            if (cmd == "start") {
                ok = mc.start(id, speed, (dir == "CCW" ? Direction::CCW : Direction::CW));
            } else if (cmd == "stop") {
                ok = mc.stop(id);
            } else if (cmd == "set") {
                ok = mc.set(id, speed, (dir == "CCW" ? Direction::CCW : Direction::CW));
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
