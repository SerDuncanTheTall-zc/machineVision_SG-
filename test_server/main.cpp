#include <iostream>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include "gesture.pb.h"

uint64_t get_timestamp_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

int main() {
    const int SERVER_PORT = 8888;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    struct sockaddr_in serv_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY; // 监听所有网卡
    serv_addr.sin_port = htons(SERVER_PORT);

    if (bind(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("绑定失败");
        return -1;
    }

    std::cout << "🏠 Server 已就绪，等待 PC 客户端连接 (端口: " << SERVER_PORT << ")..." << std::endl;

    // --- 第一阶段：等待客户端“拍一拍” ---
    char buffer[1024];
    int n = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &client_len);
    
    std::cout << "📢 侦测到客户端请求！IP: " << inet_ntoa(client_addr.sin_addr) 
              << " Port: " << ntohs(client_addr.sin_port) << std::endl;
    std::cout << "🎬 开始直播手势数据..." << std::endl;

    // --- 第二阶段：开始发送数据 (逻辑同前，但目标地址是动态获取的) ---
    int frame_id = 0;
    while (true) {
        gesture::FramePayload payload;
        payload.set_timestamp(get_timestamp_ms());
        payload.set_frame_id(++frame_id);

        gesture::Hand* hand = payload.add_hands();
        hand->set_id(1);
        hand->set_label("Server_Mode"); // 标记一下模式
        hand->set_confidence(0.99f);
        
        // 让框动起来方便观察
        float offset = (frame_id % 100) / 200.0f;
        hand->mutable_box_min()->set_x(0.1f + offset);
        hand->mutable_box_min()->set_y(0.2f);
        hand->mutable_box_max()->set_x(0.3f + offset);
        hand->mutable_box_max()->set_y(0.5f);

        std::string out;
        if (payload.SerializeToString(&out)) {
            sendto(sock, out.c_str(), out.length(), 0, (struct sockaddr*)&client_addr, client_len);
        }
        usleep(33000); 
    }

    close(sock);
    return 0;
}