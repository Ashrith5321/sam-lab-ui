#include "HttpServer.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>

static std::string urlDecode(const std::string &s){
    std::string o; o.reserve(s.size());
    for(size_t i=0;i<s.size();++i){
        if(s[i]=='%' && i+2<s.size()){
            int v = 0; std::stringstream ss; ss << std::hex << s.substr(i+1,2); ss >> v; o.push_back((char)v); i+=2;
        } else if (s[i]=='+') o.push_back(' ');
        else o.push_back(s[i]);
    }
    return o;
}

static std::string guessType(const std::string &path){
    if (path.rfind(".html")!=std::string::npos) return "text/html";
    if (path.rfind(".js")!=std::string::npos) return "application/javascript";
    if (path.rfind(".css")!=std::string::npos) return "text/css";
    return "text/plain";
}

HttpServer::HttpServer() {}
HttpServer::~HttpServer(){ stop(); }

bool HttpServer::start(unsigned short port, const std::string &staticDir, Handler handler){
    staticDir_ = staticDir; handler_ = handler;
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) { perror("socket"); return false; }

    int opt=1; setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_ANY); addr.sin_port = htons(port);
    if (bind(server_fd_, (sockaddr*)&addr, sizeof(addr))<0){ perror("bind"); ::close(server_fd_); return false; }
    if (listen(server_fd_, 16)<0){ perror("listen"); ::close(server_fd_); return false; }

    running_ = true;
    th_ = std::thread([this, port]{
        std::cerr << "HTTP listening on http://127.0.0.1:" << port << "\n";
        while(running_){
            int cfd = accept(server_fd_, nullptr, nullptr);
            if (cfd < 0) { if (running_) perror("accept"); continue; }
            std::thread([this, cfd]{
                char buf[8192];
                ssize_t n = read(cfd, buf, sizeof(buf)-1);
                if (n<=0){ ::close(cfd); return; }
                buf[n]=0;
                std::string req(buf);
                std::istringstream is(req);
                std::string method, path, version; is >> method >> path >> version;
                std::string headers, line; std::getline(is, line); // rest of first line
                size_t pos = req.find("\r\n\r\n");
                std::string body = (pos!=std::string::npos) ? req.substr(pos+4) : std::string();

                int status=200; std::string contentType="text/plain"; std::string out;

                // API routes under /api
                if (path.rfind("/api",0)==0 && handler_){
                    out = handler_(method, urlDecode(path), body, status, contentType);
                } else {
                    std::string spath = path;
                    if (spath=="/") spath = "/index.html";
                    std::ifstream f(staticDir_ + spath, std::ios::binary);
                    if (f){
                        std::ostringstream ss; ss << f.rdbuf(); out = ss.str();
                        contentType = guessType(spath);
                    } else {
                        status = 404; out = "Not Found";
                    }
                }

                std::ostringstream resp;
                resp << "HTTP/1.1 " << status << "\r\n";
                resp << "Content-Type: " << contentType << "\r\n";
                resp << "Content-Length: " << out.size() << "\r\n";
                resp << "Connection: close\r\n\r\n";
                resp << out;
                auto s = resp.str();
                write(cfd, s.data(), s.size());
                ::close(cfd);
            }).detach();
        }
    });
    return true;
}

void HttpServer::stop(){
    if (!running_) return;
    running_ = false;
    if (server_fd_>=0) { ::shutdown(server_fd_, SHUT_RDWR); ::close(server_fd_); server_fd_ = -1; }
    if (th_.joinable()) th_.join();
}
