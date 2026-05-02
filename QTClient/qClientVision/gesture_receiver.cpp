#include "gesture_receiver.h"
#include <QDebug>
#include "gesture.pb.h" // 引入生成的 Protobuf 头文件

GestureReceiver::GestureReceiver(QObject *parent)
    : QObject(parent), m_isRunning(false)
{
    m_udpSocket = new QUdpSocket(this);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &GestureReceiver::onReadyRead);
}

GestureReceiver::~GestureReceiver()
{
    stop();
}

bool GestureReceiver::start(quint16 port)
{
    if (m_udpSocket->bind(QHostAddress::AnyIPv4, port)) {
        m_isRunning = true;
        qDebug() << "UDP 接收器已启动，监听端口:" << port;
        return true;
    }
    qDebug() << "UDP 端口绑定失败!";
    return false;
}

void GestureReceiver::stop()
{
    if (m_isRunning) {
        m_udpSocket->close();
        m_isRunning = false;
        qDebug() << "UDP 接收器已停止";
    }
}

bool GestureReceiver::isRunning() const
{
    return m_isRunning;
}

void GestureReceiver::onReadyRead()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());

        QHostAddress sender;
        quint16 senderPort;
        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        // 使用 Protobuf 解析二进制流
        gesture::FramePayload payload;
        if (payload.ParseFromArray(datagram.data(), datagram.size())) {

            QList<HandData> handsList;
            uint64_t ts = payload.timestamp();

            // 遍历所有检测到的手
            for (int i = 0; i < payload.hands_size(); ++i) {
                const auto& hand = payload.hands(i);

                HandData hd;
                hd.id = hand.id();
                hd.label = QString::fromStdString(hand.label());
                hd.confidence = hand.confidence();

                // 计算归一化的矩形 (x, y, width, height)
                float x = hand.box_min().x();
                float y = hand.box_min().y();
                float w = hand.box_max().x() - x;
                float h = hand.box_max().y() - y;
                hd.rect = QRectF(x, y, w, h);

                handsList.append(hd);
            }

            // 发射信号给 UI 线程
            emit dataReceived(ts, handsList);
        } else {
            qWarning() << "Protobuf 反序列化失败，收到无效数据包，大小:" << datagram.size();
        }
    }
}
