#ifndef VIDEO_CANVAS_H
#define VIDEO_CANVAS_H

#include <QWidget>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include "gesture_receiver.h" // 引用 HandData 结构体

/**
 * @brief 视频渲染画布
 * 采用双层渲染架构：底层绘制 GStreamer 解码后的视频，顶层绘制 Protobuf 坐标框
 */
class VideoCanvas : public QWidget
{
    Q_OBJECT
public:
    explicit VideoCanvas(QWidget* parent = nullptr);

public slots:
    // 接收并刷新背景视频帧（由 GstVideoReceiver 触发）
    void updateVideoFrame(const QImage& frame);

    // 更新手势识别坐标数据（由 GestureReceiver 触发）
    void updateOverlay(const QList<HandData>& hands);

    // 动态控制是否显示 AI 标注层
    void setDrawOverlay(bool enable);

protected:
    // 核心渲染逻辑
    void paintEvent(QPaintEvent* event) override;

private:
    QImage m_currentFrame;          // 当前视频帧缓存
    QList<HandData> m_hands;        // 当前手势列表缓存
    bool m_drawOverlay;             // 绘制开关
    QMutex m_mutex;                 // 跨线程数据保护锁
};

#endif // VIDEO_CANVAS_H