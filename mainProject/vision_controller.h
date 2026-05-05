#ifndef VISION_CONTROLLER_H
#define VISION_CONTROLLER_H

#include <memory>
#include <string>
#include <atomic>
#include "tcp_server.h"
#include "gst_streamer.h"
#include "yolo_inference.h"
#include "servo_manager.h"

class VisionController {
public:
    VisionController();
    ~VisionController();

    // 初始化所有子模块
    bool init(const std::string& model_path, const std::string& client_ip, int udp_port, int tcp_port);
    
    // 启动主业务循环
    void run();
    
    // 停止所有业务
    void stop();

private:
    // 处理来自 TCP Server 的控制指令 (如开启追踪模式)
    void handleClientCommand(const VisionData::ControlCommand& cmd);
    
    // 处理推理结果并发送给客户端
    void processInferenceResult(const std::vector<Detection>& detections);

    // 子模块实例
    std::unique_ptr<TcpServer> tcp_server_;
    std::unique_ptr<GstStreamer> gst_streamer_;
    std::unique_ptr<YoloInference> yolo_engine_;
    std::unique_ptr<ServoManager> servo_hw_;

    std::atomic<bool> is_running_;
    std::atomic<bool> is_tracking_mode_; // 是否开启视觉追踪模式
};

#endif