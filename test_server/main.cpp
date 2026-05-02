#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include "gesture.pb.h"

// 获取当前毫秒时间戳
uint64_t get_timestamp_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// 简单的模拟对象结构体
struct SimulatedHand {
    int id;
    std::string label;
    float x, y;       // 中心点坐标
    float vx, vy;     // 速度向量
    float size;       // 框的大小
};

int main() {
    // 🚩 确认这是你 Windows 的实际 IP
    const char* PC_IP = "192.168.2.101"; 
    const int PC_PORT = 8888;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket 创建失败");
        return -1;
    }

    struct sockaddr_in pc_addr;
    pc_addr.sin_family = AF_INET;
    pc_addr.sin_port = htons(PC_PORT);
    pc_addr.sin_addr.s_addr = inet_addr(PC_IP);

    std::cout << "🚀 动态手势模拟器已启动 -> 推送至 " << PC_IP << ":" << PC_PORT << std::endl;

    // 初始化三个模拟对象
    std::vector<SimulatedHand> hands = {
        {1, "Crossover", 0.2f, 0.2f, 0.015f, 0.012f, 0.15f},
        {2, "Step-Back", 0.5f, 0.8f, -0.010f, -0.018f, 0.12f},
        {3, "Fadeaway",  0.8f, 0.3f, 0.020f, -0.008f, 0.18f}
    };

    int frame_id = 0;
    while (true) {
        gesture::FramePayload payload;
        payload.set_timestamp(get_timestamp_ms());
        payload.set_frame_id(++frame_id);

        for (auto& h : hands) {
            // 1. 物理更新：位置 = 位置 + 速度
            h.x += h.vx;
            h.y += h.vy;

            // 2. 边界碰撞检测 (归一化范围 0.0 ~ 1.0)
            float half_s = h.size / 2.0f;
            if (h.x - half_s < 0 || h.x + half_s > 1.0f) h.vx *= -1; // X 轴反弹
            if (h.y - half_s < 0 || h.y + half_s > 1.0f) h.vy *= -1; // Y 轴反弹

            // 3. 填充 Protobuf 数据
            gesture::Hand* hand_msg = payload.add_hands();
            hand_msg->set_id(h.id);
            hand_msg->set_label(h.label);
            
            // 模拟一个随抖动的置信度 (0.90 ~ 0.99)
            float conf = 0.90f + static_cast<float>(rand() % 100) / 1000.0f;
            hand_msg->set_confidence(conf);

            // 设置检测框 (min 和 max)
            hand_msg->mutable_box_min()->set_x(h.x - half_s);
            hand_msg->mutable_box_min()->set_y(h.y - half_s);
            hand_msg->mutable_box_max()->set_x(h.x + half_s);
            hand_msg->mutable_box_max()->set_y(h.y + half_s);
        }

        // 4. 序列化
        std::string out;
        if (payload.SerializeToString(&out)) {
            sendto(sock, out.c_str(), out.length(), 0,
                   (struct sockaddr*)&pc_addr, sizeof(pc_addr));
            
            std::cout << "\r[Frame " << frame_id << "] 正在场上运球... 发送字节: " << out.length() << "  " << std::flush;
        }

        usleep(33000); // 维持在 30 FPS 左右
    }

    close(sock);
    return 0;
}