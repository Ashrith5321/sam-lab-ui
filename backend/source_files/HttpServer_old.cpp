// backend/HttpServer.cpp
#include "HttpServer.hpp"
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm> // std::min
#include <cctype>

// --- helpers ---------------------------------------------------------------

static bool send_all(int fd, const std::string& data) {
  size_t sent = 0;
  while (sent < data.size()) {
    ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, 0);
    if (n <= 0) return false;
    sent += (size_t)n;
  }
  return true;
}

static std::string http_ok_json(const std::string& json) {
  std::ostringstream oss;
  oss << "HTTP/1.1 200 OK\r\n"
      << "Content-Type: application/json\r\n"
      << "Content-Length: " << json.size() << "\r\n"
      << "Connection: close\r\n\r\n"
      << json;
  return oss.str();
}

static std::string http_ok_html(const std::string& html) {
  std::ostringstream oss;
  oss << "HTTP/1.1 200 OK\r\n"
      << "Content-Type: text/html; charset=utf-8\r\n"
      << "Content-Length: " << html.size() << "\r\n"
      << "Connection: close\r\n\r\n"
      << html;
  return oss.str();
}

static std::string http_404() {
  std::string body = "Not Found";
  std::ostringstream oss;
  oss << "HTTP/1.1 404 Not Found\r\n"
      << "Content-Type: text/plain\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n\r\n"
      << body;
  return oss.str();
}

static std::string http_400(const std::string& msg) {
  std::ostringstream oss;
  oss << "HTTP/1.1 400 Bad Request\r\n"
      << "Content-Type: text/plain\r\n"
      << "Content-Length: " << msg.size() << "\r\n"
      << "Connection: close\r\n\r\n"
      << msg;
  return oss.str();
}

// --- class impl ------------------------------------------------------------

HttpServer::HttpServer(const std::string& host, int port, Handlers h)
  : host_(host), port_(port), h_(std::move(h)) {}

HttpServer::~HttpServer() { stop(); }

bool HttpServer::start() {
  if (running_.exchange(true)) return true;
  thr_ = std::thread(&HttpServer::serverLoop, this);
  return true;
}

void HttpServer::stop() {
  running_ = false;
  if (thr_.joinable()) thr_.join();
}

void HttpServer::serverLoop() {
  int srv = ::socket(AF_INET, SOCK_STREAM, 0);
  if (srv < 0) { perror("socket"); return; }

  int yes = 1;
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{}; addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port_);
  addr.sin_addr.s_addr = inet_addr(host_.c_str()); // e.g. "127.0.0.1"

  if (bind(srv, (sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("bind"); close(srv); return;
  }
  if (listen(srv, 16) < 0) {
    perror("listen"); close(srv); return;
  }

  std::cerr << "HTTP listening on http://" << host_ << ":" << port_ << "\n";

  while (running_) {
    sockaddr_in cli{}; socklen_t clilen = sizeof(cli);
    int fd = accept(srv, (sockaddr*)&cli, &clilen);
    if (fd < 0) { usleep(1000 * 50); continue; }

    // Label allows early-exit on bad socket read
  next_client:
    if (fd < 0) continue;

    // --------- Robust header + body read (drop-in replacement) ----------
    std::string raw, head, body;
    char buf[1024];

    // Read until we see the end of headers (\r\n\r\n)
    for (;;) {
      ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
      if (n <= 0) { close(fd); goto next_client; }
      raw.append(buf, n);
      size_t pos = raw.find("\r\n\r\n");
      if (pos != std::string::npos) {
        head = raw.substr(0, pos + 4);         // headers incl. CRLFCRLF
        body = raw.substr(pos + 4);            // any body bytes already received
        break;
      }
      // keep reading until headers complete
    }

    // Parse request line
    std::istringstream hss(head);
    std::string request_line;
    std::getline(hss, request_line);
    if (!request_line.empty() && request_line.back() == '\r') request_line.pop_back();
    std::istringstream rl(request_line);
    std::string method, path, version;
    rl >> method >> path >> version;

    // Parse headers (case-insensitive), extract Content-Length
    size_t cl = 0;
    std::string header_line;
    while (std::getline(hss, header_line)) {
      if (!header_line.empty() && header_line.back() == '\r') header_line.pop_back();
      std::string low = header_line;
      for (auto &c : low) c = std::tolower(static_cast<unsigned char>(c));
      if (low.rfind("content-length:", 0) == 0) {
        // skip "content-length:" (15 chars) and whitespace
        std::string val = header_line.substr(15);
        // trim leading spaces
        size_t i = 0; while (i < val.size() && std::isspace(static_cast<unsigned char>(val[i]))) ++i;
        cl = (size_t)std::strtoul(val.c_str() + i, nullptr, 10);
      }
    }

    // If body not fully received yet, read the remainder
    if (body.size() < cl) {
      size_t need = cl - body.size();
      while (need > 0) {
        ssize_t n = ::recv(fd, buf, std::min(sizeof(buf), need), 0);
        if (n <= 0) break;
        body.append(buf, n);
        need -= (size_t)n;
      }
    }

    std::cerr << "[HTTP] " << method << " " << path << "\n";
    std::cerr << "[HTTP] Content-Length=" << cl << ", body='" << body << "'\n";
    // --------- End replacement ----------

    // ----------------- Routes -----------------

    // Serve index
    if (method == "GET" && (path == "/" || path == "/index.html")) {
      std::string file = h_.getIndexPath ? h_.getIndexPath() : "";
      std::ifstream in(file, std::ios::binary);
      if (!in) { send_all(fd, http_404()); close(fd); continue; }
      std::ostringstream data; data << in.rdbuf();
      send_all(fd, http_ok_html(data.str()));
      close(fd); continue;
    }

    // Status
    if (method == "GET" && path == "/status") {
      std::string j = h_.getStatusJson ? h_.getStatusJson() : "{\"ok\":false}";
      send_all(fd, http_ok_json(j));
      close(fd); continue;
    }

    // Convenience routes to bypass POST body parsing during testing
    if (method == "GET" && path == "/on") {
      std::string j = h_.postMotorJson ? h_.postMotorJson("state=on") : "{\"ok\":false}";
      send_all(fd, http_ok_json(j));
      close(fd); continue;
    }
    if (method == "GET" && path == "/off") {
      std::string j = h_.postMotorJson ? h_.postMotorJson("state=off") : "{\"ok\":false}";
      send_all(fd, http_ok_json(j));
      close(fd); continue;
    }

    // Control endpoint
    if (method == "POST" && path == "/motor") {
      if (!h_.postMotorJson) {
        send_all(fd, http_400("No handler")); close(fd); continue;
      }
      std::string j = h_.postMotorJson(body);
      if (j.empty()) j = "{\"ok\":false}";
      send_all(fd, http_ok_json(j));
      close(fd); continue;
    }

    // Fallback 404
    send_all(fd, http_404());
    close(fd);
  }

  close(srv);
}
