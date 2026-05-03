#include "video_canvas.h"
#include <QPainter>
#include <QPen>
#include <QFont>
#include <QDebug>

VideoCanvas::VideoCanvas(QWidget* parent)
    : QWidget(parent), m_drawOverlay(true)
{
    // 性能优化：告诉 Qt 这是一个不透明部件，减少底层重绘
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);

    // 设置深色背景，流未连接时显得更专业
    this->setStyleSheet("background-color: #000000;");
}

void VideoCanvas::updateVideoFrame(const QImage& frame)
{
    QMutexLocker locker(&m_mutex);
    // 使用 .copy() 确保在多线程环境下图像数据的完整性
    m_currentFrame = frame.copy();
    locker.unlock();

    update(); // 请求界面重绘
}

void VideoCanvas::updateOverlay(const QList<HandData>& hands)
{
    QMutexLocker locker(&m_mutex);
    m_hands = hands;
    locker.unlock();

    update(); // 数据到达后刷新，确保框的移动频率能跟上数据流
}

void VideoCanvas::setDrawOverlay(bool enable)
{
    m_drawOverlay = enable;
    update();
}

void VideoCanvas::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    // 开启抗锯齿，让边缘更平滑
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QMutexLocker locker(&m_mutex);

    // --- 1. 绘制底层视频流 ---
    if (!m_currentFrame.isNull()) {
        // 将 1080p 视频拉伸铺满当前窗口（也可以根据需求使用 KeepAspectRatio）
        painter.drawImage(this->rect(), m_currentFrame);
    }
    else {
        // 无信号时的提示
        painter.fillRect(this->rect(), Qt::black);
        painter.setPen(Qt::gray);
        painter.drawText(this->rect(), Qt::AlignCenter, "WAITING FOR STREAM...");
    }

    // --- 2. 绘制顶层 AI 标注层 ---
    if (m_drawOverlay && !m_hands.isEmpty()) {
        QPen pen(QColor(0, 255, 0), 3); // 经典 AI 绿
        painter.setPen(pen);

        QFont font("Consolas", 11, QFont::Bold);
        painter.setFont(font);

        int winW = this->width();
        int winH = this->height();

        for (const auto& hand : m_hands) {
            // 归一化坐标转换 (0.0~1.0 -> 像素)
            int x = static_cast<int>(hand.rect.x() * winW);
            int y = static_cast<int>(hand.rect.y() * winH);
            int w = static_cast<int>(hand.rect.width() * winW);
            int h = static_cast<int>(hand.rect.height() * winH);

            QRect targetBox(x, y, w, h);

            // 绘制主检测框
            painter.drawRect(targetBox);

            // 绘制标签与置信度
            QString labelText = QString("%1 %2%").arg(hand.label).arg(static_cast<int>(hand.confidence * 100));

            // 计算标签背景尺寸，增加易读性
            QRect textRect = painter.fontMetrics().boundingRect(labelText);
            textRect.moveTo(x, y - textRect.height() - 5);
            textRect.adjust(-4, 0, 4, 2); // 留出一点边距

            // 画半透明标签底
            painter.fillRect(textRect, QColor(0, 255, 0, 180));

            // 画文字（黑色避开高亮底色）
            painter.setPen(Qt::black);
            painter.drawText(textRect, Qt::AlignCenter, labelText);

            // 恢复画笔颜色用于下一个目标
            painter.setPen(pen);
        }
    }

    // 渲染完毕后，locker 会自动释放
}