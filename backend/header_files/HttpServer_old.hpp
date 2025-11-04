#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <functional>

class HttpServer {
public:
  struct Handlers {
    // GET /status -> JSON string
    std::function<std::string()> getStatusJson;
    // POST /motor with body -> returns JSON string
    std::function<std::string(const std::string& body)> postMotorJson;
    // GET / -> returns file path of index.html to serve
    std::function<std::string()> getIndexPath;
  };

  HttpServer(const std::string& host, int port, Handlers h);
  ~HttpServer();

  bool start();
  void stop();

private:
  void serverLoop();

  std::string host_;
  int port_;
  Handlers h_;
  std::thread thr_;
  std::atomic<bool> running_{false};
};
