#ifndef GST_VIDEO_RECEIVER_H
#define GST_VIDEO_RECEIVER_H

#include <QObject>
#include <QImage>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

/**
 * @brief GStreamer 视频接收与解码类
 * 专门针对 Windows MSVC + Intel/AMD/NVIDIA 硬件解码优化
 */
class GstVideoReceiver : public QObject {
    Q_OBJECT
public:
    explicit GstVideoReceiver(QObject* parent = nullptr);
    ~GstVideoReceiver();

    // 启动拉流：port 通常为 5000
    void start(int port);

    // 停止并清理管道
    void stop();

signals:
    // 解码后的 RGBA 图像信号
    void frameReady(const QImage& frame);

private:
    // GStreamer appsink 回调函数
    static GstFlowReturn on_new_sample(GstElement* sink, gpointer user_data);

    GstElement* m_pipeline = nullptr;
};

#endif // GST_VIDEO_RECEIVER_H