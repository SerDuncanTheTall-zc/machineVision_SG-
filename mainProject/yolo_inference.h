#ifndef YOLO_INFERENCE_H
#define YOLO_INFERENCE_H

#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
#include "rknn_api.h"

// 检测结果结构体
struct Detection {
    float x, y, w, h;     // 归一化坐标 (0.0 - 1.0)
    float confidence;      // 置信度
    int class_id;          // 类别索引
    std::string label;     // 类别名称
};

class YoloInference {
public:
    YoloInference();
    ~YoloInference();

    // 加载 RKNN 模型并初始化 NPU 算力上下文
    bool loadModel(const std::string& model_path);

    // 执行推理并返回检测到的目标列表
    std::vector<Detection> infer(cv::Mat& frame);

private:
    // YOLOv8 特有的后处理：处理 Anchor-free 输出张量
    void postProcess(int8_t* out0, float* out1, float* out2, std::vector<Detection>& results);
    
    // 释放资源
    void release();

    rknn_context ctx_;
    rknn_input_output_num io_num_;
    rknn_tensor_attr* input_attrs_;
    rknn_tensor_attr* output_attrs_;

    int model_width_ = 640;
    int model_height_ = 640;
    int model_channels_ = 3;
};

#endif