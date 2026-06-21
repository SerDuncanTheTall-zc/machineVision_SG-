#ifndef GST_STREAMER_H
#define GST_STREAMER_H

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <mutex>

class GstStreamer {
public:
    GstStreamer();
    ~GstStreamer();

    // 初始化硬件加速 Pipeline: 一路推流到目标IP，一路给本地推理
    bool initPipeline(const std::string& targetIp, int port);
    
    // 启动管道
    void start();
    
    // 停止管道并释放资源
    void stop();

    // 从 appsink 中拉取最新的图像帧供 YOLO 推理
    cv::Mat pullFrame();

private:
    // 融入 Python 脚本中的硬件重置逻辑
    void resetHardware();

    GstElement* pipeline_;
    GstElement* appsink_;
    
    // 用于确保多线程状态下 pipeline 操作的安全
    std::mutex mutex_;
};

#endif