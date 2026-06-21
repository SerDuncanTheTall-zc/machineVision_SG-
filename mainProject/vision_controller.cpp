#include "vision_controller.h"
#include <chrono>

VisionController::VisionController() 
    : is_running_(false), is_tracking_mode_(false) {}

VisionController::~VisionController() { stop(); }

bool VisionController::init(const std::string& model_path, const std::string& client_ip, int udp_port, int tcp_port) {
    // 1. 初始化推理引擎 (YOLOv8)
    yolo_engine_ = std::make_unique<YoloInference>();
    if (!yolo_engine_->loadModel(model_path)) return false;

    // 2. 初始化推流器
    gst_streamer_ = std::make_unique<GstStreamer>();
    if (!gst_streamer_->initPipeline(client_ip, udp_port)) return false;

    // 3. 初始化控制服务器
    tcp_server_ = std::make_unique<TcpServer>(tcp_port);
    tcp_server_->setCommandCallback([this](const VisionData::ControlCommand& cmd) {
        this->handleClientCommand(cmd);
    });

    // 4. 初始化舵机控制 (预留接口)
    servo_hw_ = std::make_unique<ServoManager>();

    return tcp_server_->start();
}

void VisionController::run() {
    is_running_ = true;
    gst_streamer_->start();

    while (is_running_) {
        // 从 GStreamer 的 appsink 获取一帧
        cv::Mat frame = gst_streamer_->pullFrame();
        if (frame.empty()) continue;

        // 执行 YOLOv8 推理
        std::vector<Detection> results = yolo_engine_->infer(frame);

        // 如果开启了追踪模式，计算舵机偏差
        if (is_tracking_mode_ && !results.empty()) {
            // 取第一个目标进行追踪逻辑 (PID)
            servo_hw_->updateTracking(results[0].x, results[0].y);
        }

        // 打包 Protobuf 并通过 TCP 发送给 QT 客户端
        processInferenceResult(results);
    }
}

void VisionController::handleClientCommand(const VisionData::ControlCommand& cmd) {
    if (cmd.type() == VisionData::ControlCommand::START_TRACKING) {
        is_tracking_mode_ = true;
        std::cout << "[Controller] 开启视觉追踪模式" << std::endl;
    } else if (cmd.type() == VisionData::ControlCommand::STOP) {
        is_tracking_mode_ = false;
        std::cout << "[Controller] 停止追踪" << std::endl;
    }
}

void VisionController::processInferenceResult(const std::vector<Detection>& detections) {
    VisionData::FramePacket packet;
    packet.set_timestamp(std::chrono::system_clock::now().time_since_epoch().count());

    for (const auto& det : detections) {
        auto* res = packet.add_results();
        res->set_label(det.label);
        res->set_confidence(det.confidence);
        res->set_x(det.x); // 建议此处已是归一化坐标
        res->set_y(det.y);
        res->set_width(det.w);
        res->set_height(det.h);
    }

    tcp_server_->sendFrameResult(packet);
}

void VisionController::stop() {
    is_running_ = false;
    if (gst_streamer_) gst_streamer_->stop();
    if (tcp_server_) tcp_server_->stop();
}