#include "gst_video_receiver.h"
#include <QDebug>
#include <QImage>
#include <gst/app/gstappsink.h>

GstVideoReceiver::GstVideoReceiver(QObject* parent) : QObject(parent) {
    // 初始化 GStreamer 环境
    // 注意：如果是生产环境，gst_init 建议放在 main.cpp 中调用一次
    if (!gst_is_initialized()) {
        gst_init(nullptr, nullptr);
    }
}

GstVideoReceiver::~GstVideoReceiver() {
    stop();
}

void GstVideoReceiver::start(int port) {
    stop(); // 启动前确保旧管道已清理

    GError* error = nullptr;
    QString pipeStr;

    // --- 一键切换开关 ---
#if 0
    // 方案 A: D3D11 模式 (稳定性高，跨显卡兼容性好)
    pipeStr = QString(
        "udpsrc port=%1 ! "
        "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96 ! "
        "rtph264depay ! h264parse ! d3d11h264dec ! videoconvert ! "
        "video/x-raw, format=RGBA ! "
        "appsink name=mysink sync=false emit-signals=true"
    ).arg(port);
    qDebug() << ">>> 当前模式: D3D11 硬件解码";
#else
// 方案 B: NVIDIA 模式 (修正版)
    pipeStr = QString(
        "udpsrc port=%1 ! "
        "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96 ! "
        "rtph264depay ! h264parse ! "
        "nvh264dec ! "                   // 删掉了 low-latency=true
        "cudadownload ! "                // 搬运逻辑保留
        "videoconvert ! "
        "video/x-raw, format=RGBA ! "
        "appsink name=mysink sync=false emit-signals=true"
    ).arg(port);
    qDebug() << ">>> 当前模式: NVIDIA CUDA 硬件解码 (已移除低延迟属性测试)";
#endif

    // 1. 构造管道并捕获解析错误
    m_pipeline = gst_parse_launch(pipeStr.toUtf8().constData(), &error);

    if (error) {
        qDebug() << ">>> [CRITICAL] 管道构造解析失败！";
        qDebug() << ">>> GStreamer 报告:" << error->message;
        g_error_free(error);
        return;
    }

    if (m_pipeline) {
        // 2. 获取 appsink 并连接信号
        GstElement* appsink = gst_bin_get_by_name(GST_BIN(m_pipeline), "mysink");
        if (appsink) {
            g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), this);
            gst_object_unref(appsink);
        }
        else {
            qDebug() << ">>> [ERROR] 管道中找不到 mysink，请检查字符串拼写";
            return;
        }

        // 3. 尝试启动管道
        GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            qDebug() << ">>> [ERROR] 无法将管道设置为 PLAYING 状态，硬件资源可能被占用";
            return;
        }

        // 4. 生成拓扑图（方便对比 A/B 方案的差异）
        // GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(m_pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "pipeline_debug");

        qDebug() << ">>> PC 硬件解码器已成功启动，监听端口:" << port;
    }
}

void GstVideoReceiver::stop() {
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        qDebug() << "GStreamer pipeline stopped.";
    }
}

GstFlowReturn GstVideoReceiver::on_new_sample(GstElement* sink, gpointer user_data) {
    auto* self = static_cast<GstVideoReceiver*>(user_data);

    // 1. 尝试拉取数据
    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample) {
        qDebug() << ">>> [ERROR] 无法获取 Sample";
        return GST_FLOW_ERROR;
    }

    // 只要执行到这里，说明网络是通的，数据包已经进到 PC 了
    // qDebug() << ">>> [INFO] 收到一帧原始数据包"; 

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;

    // 2. 核心：尝试映射内存。
    // 如果没有 cudadownload，nvh264dec 产出的 buffer 留在显存，CPU 映射必然失败
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        qDebug() << ">>> [CRITICAL] 内存映射失败！数据还在 GPU 显存里，没能搬运到系统内存。";
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    // 3. 获取图像元数据
    GstCaps* caps = gst_sample_get_caps(sample);
    GstStructure* s = gst_caps_get_structure(caps, 0);
    int width = 0, height = 0;

    if (gst_structure_get_int(s, "width", &width) && gst_structure_get_int(s, "height", &height)) {
        // 4. 构建 QImage
        // 必须 .copy()，因为 map.data 在 unmap 后会变为野指针
        QImage img(map.data, width, height, QImage::Format_RGBA8888);
        if (!img.isNull()) {
            emit self->frameReady(img.copy());
        }
    }
    else {
        qDebug() << ">>> [WARN] Caps 中缺少宽高信息";
    }

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}