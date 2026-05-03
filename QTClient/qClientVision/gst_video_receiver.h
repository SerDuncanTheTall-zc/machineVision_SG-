#ifndef GST_VIDEO_RECEIVER_H
#define GST_VIDEO_RECEIVER_H

#include <QObject>
#include <QImage>
#include <gst/gst.h>

#include <gst/app/gstappsink.h>

class GstVideoReceiver : public QObject {
    Q_OBJECT
public:
    explicit GstVideoReceiver(QObject *parent = nullptr);
    ~GstVideoReceiver();

    void start(int port);
    void stop();

signals:
    void frameReady(const QImage &frame);

private:
    static GstFlowReturn on_new_sample(GstElement *sink, gpointer user_data);

    GstElement *m_pipeline = nullptr;
    bool m_initialized = false;
};

#endif