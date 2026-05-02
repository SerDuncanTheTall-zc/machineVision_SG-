#include "video_canvas.h"
#include <QPainter>
#include <QPen>
#include <QFont>

VideoCanvas::VideoCanvas(QWidget *parent)
    : QWidget(parent), m_drawOverlay(true)
{
    // 设置背景为黑色，看起来更像播放器
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setStyleSheet("background-color: black;");
}

void VideoCanvas::updateVideoFrame(const QImage &frame)
{
    QMutexLocker locker(&m_mutex);
    // 这里如果视频流是连续的，可以直接赋值并 update()
    // 注意：需要确保传入的 QImage 格式正确且不需要深拷贝（除非跨线程需要）
    m_currentFrame = frame.copy();
    locker.unlock();

    update(); // 触发重绘
}

void VideoCanvas::updateOverlay(const QList<HandData> &hands)
{
    QMutexLocker locker(&m_mutex);
    m_hands = hands;
    locker.unlock();

    update(); // 触发重绘
}

void VideoCanvas::setDrawOverlay(bool enable)
{
    m_drawOverlay = enable;
    update();
}

void VideoCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    // 为了平滑缩放，开启抗锯齿
    painter.setRenderHint(QPainter::Antialiasing);

    QMutexLocker locker(&m_mutex);

    // 1. 绘制底层视频画面
    if (!m_currentFrame.isNull()) {
        // 将视频画面等比例缩放到窗口大小并居中
        QRect targetRect = m_currentFrame.rect();
        targetRect.moveCenter(this->rect().center());

        // 简单暴力：直接拉伸铺满全屏 (实际开发中可以根据需求改为 KeepAspectRatio)
        painter.drawImage(this->rect(), m_currentFrame);
    } else {
        // 如果没有视频流，画个黑底
        painter.fillRect(this->rect(), Qt::black);
    }

    // 2. 绘制顶层标注框
    if (m_drawOverlay) {
        QPen pen(Qt::green, 3);
        painter.setPen(pen);
        QFont font("Arial", 12, QFont::Bold);
        painter.setFont(font);

        int winWidth = this->width();
        int winHeight = this->height();

        for (const auto& hand : m_hands) {
            // 将 0.0~1.0 的归一化坐标还原为当前窗口的实际像素坐标
            int x = static_cast<int>(hand.rect.x() * winWidth);
            int y = static_cast<int>(hand.rect.y() * winHeight);
            int w = static_cast<int>(hand.rect.width() * winWidth);
            int h = static_cast<int>(hand.rect.height() * winHeight);

            QRect targetBox(x, y, w, h);

            // 画框
            painter.drawRect(targetBox);

            // 画背景色标签，让文字更清晰
            QString info = QString("%1 (%2%)").arg(hand.label).arg(static_cast<int>(hand.confidence * 100));
            QRect textRect = painter.fontMetrics().boundingRect(info);
            textRect.moveTo(x, y - textRect.height() - 2); // 放在框的左上角上方

            painter.fillRect(textRect.adjusted(-2, -2, 2, 2), QColor(0, 255, 0, 150)); // 半透明绿底

            painter.setPen(Qt::black); // 黑字
            painter.drawText(textRect.bottomLeft(), info);

            painter.setPen(pen); // 恢复绿笔画下一个框
        }
    }
}
