#include "gst_video_receiver.h"
#include <QDebug>

GstVideoReceiver::GstVideoReceiver(QObject *parent) : QObject(parent) {
    gst_init(nullptr, nullptr);
}

GstVideoReceiver::~GstVideoReceiver() { stop(); }

void GstVideoReceiver::start(int port) {
    // 构造 Pipeline 字符串，必须与板端的 rtph264pay 参数严格对应
    QString pipeStr = QString(
                          "udpsrc port=%1 ! "
                          "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96 ! "
                          "rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! "
                          "video/x-raw, format=RGBA ! appsink name=mysink sync=false emit-signals=true"
                          ).arg(port);

    m_pipeline = gst_parse_launch(pipeStr.toUtf8().constData(), nullptr);
    if (!m_pipeline) {
        qDebug() << "Failed to create GStreamer pipeline!";
        return;
    }

    GstElement *appsink = gst_bin_get_by_name(GST_BIN(m_pipeline), "mysink");
    g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), this);
    gst_object_unref(appsink);

    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    qDebug() << "GStreamer receiver started on port" << port;
}

void GstVideoReceiver::stop() {
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
}

GstFlowReturn GstVideoReceiver::on_new_sample(GstElement *sink, gpointer user_data) {
    auto *self = static_cast<GstVideoReceiver*>(user_data);
    GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));

    if (sample) {
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;

        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            GstCaps *caps = gst_sample_get_caps(sample);
            GstStructure *s = gst_caps_get_structure(caps, 0);
            int width, height;
            gst_structure_get_int(s, "width", &width);
            gst_structure_get_int(s, "height", &height);

            // 直接封装为 QImage，使用 RGBA 格式避免 PC 端颜色转换开销
            QImage img(map.data, width, height, QImage::Format_RGBA8888);
            emit self->frameReady(img.copy()); // 发送拷贝以保证线程安全

            gst_buffer_unmap(buffer, &map);
        }
        gst_sample_unref(sample);
    }
    return GST_FLOW_OK;
}