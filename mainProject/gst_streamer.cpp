#include "gst_streamer.h"
#include <iostream>
#include <unistd.h>

GstStreamer::GstStreamer() : pipeline_(nullptr), appsink_(nullptr) {
    gst_init(nullptr, nullptr);
}

GstStreamer::~GstStreamer() {
    stop();
}

void GstStreamer::resetHardware() {
    std::cout << "[Gst] 正在执行硬件复位与内存清理..." << std::endl;
    // 强杀残留进程，释放硬件独占锁
    system("pkill -9 gst-launch-1.0 > /dev/null 2>&1");
    // 关键：清理内核缓存以释放连续内存 (CMA)
    if (getuid() == 0) {
        system("sync && echo 3 > /proc/sys/vm/drop_caches");
    }
    sleep(1);
}

bool GstStreamer::initPipeline(const std::string& targetIp, int port) {
    resetHardware();

    // 针对 RK3576 优化的双路 Pipeline 字符串
    // 利用 tee 模块将流拆分：
    // 1. 实时硬编推流：mpph264enc -> rtph264pay -> udpsink
    // 2. 本地推理支路：videoconvert -> BGR 格式 -> appsink
    std::string pipelineDesc = 
        "v4l2src device=/dev/video73 io-mode=2 ! "
        "image/jpeg,width=1920,height=1080,framerate=30/1 ! jpegparse ! mppjpegdec ! "
        "tee name=t "
        "t. ! queue max-size-buffers=1 leaky=downstream ! mpph264enc config-interval=1 ! rtph264pay ! udpsink host=" + targetIp + " port=" + std::to_string(port) + " "
        "t. ! queue max-size-buffers=1 leaky=downstream ! videoconvert ! video/x-raw,format=BGR ! appsink name=yolo_sink emit-signals=true sync=false";

    GError* error = nullptr;
    pipeline_ = gst_parse_launch(pipelineDesc.c_str(), &error);

    if (error) {
        std::cerr << "[Gst] Pipeline 启动失败: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    // 获取 appsink 插件句柄
    appsink_ = gst_bin_get_by_name(GST_BIN(pipeline_), "yolo_sink");
    
    std::cout << "[Gst] 硬件加速 Pipeline 初始化成功，推流目标: " << targetIp << ":" << port << std::endl;
    return true;
}

void GstStreamer::start() {
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    }
}

cv::Mat GstStreamer::pullFrame() {
    if (!appsink_) return cv::Mat();

    // 从 appsink 提取样本
    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink_));
    if (!sample) return cv::Mat();

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstCaps* caps = gst_sample_get_caps(sample);
    GstStructure* s = gst_caps_get_structure(caps, 0);

    int width, height;
    gst_structure_get_int(s, "width", &width);
    gst_structure_get_int(s, "height", &height);

    GstMapInfo map;
    gst_buffer_map(buffer, &map, GST_MAP_READ);

    // 将 GStreamer 的 buffer 直接包装成 OpenCV Mat
    // 这里的图像已经是 BGR 格式，直接给 YOLO 使用
    cv::Mat frame(cv::Size(width, height), CV_8UC3, (char*)map.data);
    cv::Mat result = frame.clone(); // 深拷贝，防止 buffer 释放后数据失效

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);

    return result;
}

void GstStreamer::stop() {
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
}