#ifndef GESTURE_RECEIVER_H
#define GESTURE_RECEIVER_H

#include <QObject>
#include <QUdpSocket>
#include <QRectF>
#include <QList>
#include <QString>

// 前向声明，加快编译速度
namespace gesture {
    class FramePayload;
}

// 定义 UI 层使用的数据结构，解耦 Protobuf
struct HandData {
    int id;
    QString label;
    float confidence;
    QRectF rect; // 存储归一化坐标 (0.0~1.0)
};

class GestureReceiver : public QObject
{
    Q_OBJECT
public:
    explicit GestureReceiver(QObject *parent = nullptr);
    ~GestureReceiver();

    bool start(quint16 port);
    void stop();
    bool isRunning() const;

signals:
    // 将解析好的数据发送给 UI 层
    void dataReceived(uint64_t ts, const QList<HandData>& hands);

private slots:
    void onReadyRead();

private:
    QUdpSocket *m_udpSocket;
    bool m_isRunning;
};

#endif // GESTURE_RECEIVER_H
