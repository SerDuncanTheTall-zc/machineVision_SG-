#ifndef VIDEO_CANVAS_H
#define VIDEO_CANVAS_H

#include <QWidget>
#include <QImage>
#include <QList>
#include <QMutex>
#include "gesture_receiver.h" // 需要用到 HandData 结构体

class VideoCanvas : public QWidget
{
    Q_OBJECT
public:
    explicit VideoCanvas(QWidget *parent = nullptr);

    // 供后续 FFmpeg 或 OpenCV 调用的接口，用来刷新背景视频
    void updateVideoFrame(const QImage &frame);

public slots:
    // 更新手势坐标框
    void updateOverlay(const QList<HandData> &hands);
    // 控制是否显示坐标框
    void setDrawOverlay(bool enable);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_currentFrame;
    QList<HandData> m_hands;
    bool m_drawOverlay;
    QMutex m_mutex; // 保护数据并发访问
};

#endif // VIDEO_CANVAS_H
