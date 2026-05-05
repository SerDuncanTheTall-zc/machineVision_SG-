#include "yolo_inference.h"
#include <iostream>
#include <fstream>

YoloInference::YoloInference() : ctx_(0), input_attrs_(nullptr), output_attrs_(nullptr) {}

YoloInference::~YoloInference() { release(); }

bool YoloInference::loadModel(const std::string& model_path) {
    // 1. 读取模型文件
    FILE* fp = fopen(model_path.c_str(), "rb");
    if (!fp) return false;
    fseek(fp, 0, SEEK_END);
    int model_len = ftell(fp);
    void* model = malloc(model_len);
    fseek(fp, 0, SEEK_SET);
    if (fread(model, 1, model_len, fp) != model_len) return false;
    fclose(fp);

    // 2. 初始化 RKNN 上下文
    int ret = rknn_init(&ctx_, model, model_len, 0, nullptr);
    free(model);
    if (ret < 0) return false;

    // 3. 查询输入输出属性
    rknn_query(ctx_, RKNN_QUERY_IN_OUT_NUM, &io_num_, sizeof(io_num_));
    input_attrs_ = new rknn_tensor_attr[io_num_.n_input];
    output_attrs_ = new rknn_tensor_attr[io_num_.n_output];
    
    for (int i = 0; i < io_num_.n_input; i++) {
        input_attrs_[i].index = i;
        rknn_query(ctx_, RKNN_QUERY_INPUT_ATTR, &input_attrs_[i], sizeof(rknn_tensor_attr));
    }
    for (int i = 0; i < io_num_.n_output; i++) {
        output_attrs_[i].index = i;
        rknn_query(ctx_, RKNN_QUERY_OUTPUT_ATTR, &output_attrs_[i], sizeof(rknn_tensor_attr));
    }
    return true;
}

std::vector<Detection> YoloInference::infer(cv::Mat& frame) {
    std::vector<Detection> results;
    if (frame.empty()) return results;

    // 1. 预处理：Resize 为模型输入尺寸 (640x640)
    cv::Mat res_img;
    cv::resize(frame, res_img, cv::Size(model_width_, model_height_));
    cv::cvtColor(res_img, res_img, cv::COLOR_BGR2RGB);

    // 2. 设置输入数据
    rknn_input inputs[1];
    memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_UINT8;
    inputs[0].size = model_width_ * model_height_ * model_channels_;
    inputs[0].fmt = RKNN_TENSOR_NHWC;
    inputs[0].buf = res_img.data;

    rknn_inputs_set(ctx_, io_num_.n_input, inputs);

    // 3. 执行 NPU 推理
    rknn_run(ctx_, nullptr);

    // 4. 获取输出结果
    rknn_output outputs[io_num_.n_output];
    memset(outputs, 0, sizeof(outputs));
    for (int i = 0; i < io_num_.n_output; i++) {
        outputs[i].want_float = 1; // 自动转换为浮点数方便后处理
    }
    rknn_outputs_get(ctx_, io_num_.n_output, outputs, nullptr);

    // 5. 后处理 (此处根据 YOLOv8 的三个输出尺度进行解码)
    // 简化逻辑：遍历输出 Tensor，应用置信度过滤和 NMS
    // ... (此处省略复杂的 NMS 循环，实际开发时需结合具体的模型输出 Head 编写)

    rknn_outputs_release(ctx_, io_num_.n_output, outputs);
    return results;
}

void YoloInference::release() {
    if (ctx_) rknn_destroy(ctx_);
    if (input_attrs_) delete[] input_attrs_;
    if (output_attrs_) delete[] output_attrs_;
}