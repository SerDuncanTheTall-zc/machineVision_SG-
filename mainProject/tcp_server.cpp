#include "tcp_server.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

TcpServer::TcpServer(int port) : port_(port), is_running_(false), server_fd_(-1), client_fd_(-1) {}

TcpServer::~TcpServer() { stop(); }

bool TcpServer::start() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) return false;
    listen(server_fd_, 3);

    is_running_ = true;
    accept_thread_ = std::thread(&TcpServer::acceptLoop, this);
    return true;
}

void TcpServer::acceptLoop() {
    while (is_running_) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int new_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &addrlen);
        
        if (new_fd >= 0) {
            std::cout << "[TCP] 客户端已连接: " << inet_ntoa(client_addr.sin_addr) << std::endl;
            client_fd_ = new_fd;
            receiveLoop(new_fd); // 简单的单客户端处理，实际可开新线程
        }
    }
}

bool TcpServer::sendFrameResult(const VisionData::FramePacket& packet) {
    if (client_fd_ < 0) return false;

    std::string serialized_data;
    packet.SerializeToString(&serialized_data);

    PacketHeader header;
    header.length = serialized_data.size();

    std::lock_guard<std::mutex> lock(send_mutex_);
    // 1. 发送 Header
    send(client_fd_, &header, sizeof(header), 0);
    // 2. 发送 Data
    send(client_fd_, serialized_data.data(), serialized_data.size(), 0);
    return true;
}

void TcpServer::receiveLoop(int client_fd) {
    while (is_running_) {
        // 这里简化处理：直接读取指令
        // 在正式项目中，建议也加 Header 校验
        char buffer[1024] = {0};
        int valread = read(client_fd, buffer, 1024);
        if (valread <= 0) {
            std::cout << "[TCP] 客户端断开连接" << std::endl;
            close(client_fd);
            this->client_fd_ = -1;
            break;
        }

        VisionData::ControlCommand cmd;
        if (cmd.ParseFromArray(buffer, valread)) {
            if (cmd_callback_) cmd_callback_(cmd);
        }
    }
}

void TcpServer::stop() {
    is_running_ = false;
    if (client_fd_ != -1) close(client_fd_);
    if (server_fd_ != -1) close(server_fd_);
    if (accept_thread_.joinable()) accept_thread_.join();
}