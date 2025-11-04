#pragma once
#include <string>
#include <thread>
#include <functional>
#include <atomic>

// Minimal HTTP server: serves static files and a couple of API endpoints.
class HttpServer {
public:
    using Handler = std::function<std::string(const std::string& method, const std::string& path, const std::string& body, int &status, std::string &contentType)>;

    HttpServer();
    ~HttpServer();

    bool start(unsigned short port, const std::string &staticDir, Handler handler);
    void stop();

private:
    int server_fd_ = -1;
    std::thread th_;
    std::atomic<bool> running_{false};
    std::string staticDir_;
    Handler handler_;
};
