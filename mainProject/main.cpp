#include <iostream>
#include <csignal>
#include <memory>
#include "tcp_server.h"
#include "gst_streamer.h"
#include "vision_controller.h"

// 全局指针用于信号处理
std::unique_ptr<VisionController> g_controller;

void signalHandler(int signum) {
    std::cout << "\n[Main] 捕获信号 (" << signum << ")，正在安全退出..." << std::endl;
    if (g_controller) {
        g_controller->stop();
    }
    exit(signum);
}

int main(int argc, char* argv[]) {
    // 注册信号处理 (Ctrl+C)
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "=== RK3576 机器视觉边缘侧服务器启动 ===" << std::endl;

    // 1. 初始化控制层 (它会内部持有 TCP, Gst, Yolo 等模块)
    g_controller = std::make_unique<VisionController>();

    // 2. 配置参数 (实际开发中可以从 json 或命令行读取)
    std::string model_path = "./models/yolov8n_hand.rknn";
    std::string target_client_ip = "192.168.1.5"; // QT 客户端 IP
    int udp_port = 5000;  // 视频推流端口
    int tcp_port = 5555;  // 指令与数据端口

    // 3. 启动系统
    if (!g_controller->init(model_path, target_client_ip, udp_port, tcp_port)) {
        std::cerr << "[Main] 系统初始化失败，请检查硬件连接或模型路径。" << std::endl;
        return -1;
    }

    std::cout << "[Main] 服务已就绪。等待客户端连接并开启追踪模式..." << std::endl;

    // 4. 进入主循环 (或者让 controller 在后台线程运行)
    g_controller->run(); 

    return 0;
}