#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <netinet/in.h>
#include "protocol/data_packet.pb.h" // 编译生成的头文件

class TcpServer {
public:
    TcpServer(int port);
    ~TcpServer();

    // 启动服务器
    bool start();
    // 停止服务器
    void stop();

    // 发送识别结果给客户端 (Protobuf 序列化后发送)
    bool sendFrameResult(const VisionData::FramePacket& packet);

    // 回调函数类型：当收到客户端指令时触发
    typedef std::function<void(const VisionData::ControlCommand&)> CommandCallback;
    void setCommandCallback(CommandCallback cb) { cmd_callback_ = cb; }

private:
    void acceptLoop();
    void receiveLoop(int client_fd);

    int server_fd_;
    int port_;
    bool is_running_;
    int client_fd_; // 当前只支持单客户端连接，若需多客户端可改为 vector

    std::thread accept_thread_;
    std::mutex send_mutex_;
    CommandCallback cmd_callback_;

    // 协议头：用于处理 TCP 粘包问题
    struct PacketHeader {
        uint32_t magic = 0x524B4E4E; // "RKNN"
        uint32_t length;             // Protobuf 数据长度
    };
};

#endif