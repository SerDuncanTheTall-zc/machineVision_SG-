#ifndef SERVO_PROTOCOL_H
#define SERVO_PROTOCOL_H

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QTimer>

/**
 * @brief 二度舵机 TCP 二进制通讯协议
 *
 * 帧格式 (定长头部 + 变长载荷):
 *   [0xAA] [0x55] [len_hi] [len_lo] [cmd] [payload...] [crc8]
 *
 *   - 0xAA 0x55 : 同步头
 *   - len_hi/lo : 载荷长度 = 1(cmd) + N(payload), 大端序
 *   - cmd       : 命令字
 *   - payload   : 命令参数
 *   - crc8      : 载荷 (cmd+payload) 的 CRC-8-ATM
 *
 * 命令字定义:
 *   0x01 SERVO_MOVE    : servo_id(u8) | dir(u8) | speed(u8)
 *                         dir: 0=STOP, 1=UP, 2=DOWN, 3=LEFT, 4=RIGHT
 *   0x02 SERVO_ABS     : servo_id(u8) | pan(i16) | tilt(i16)
 *   0x03 TRACKING_CTRL : enable(u8)
 *   0x04 STREAM_CTRL   : enable(u8) | udp_data_port(u16) | udp_video_port(u16)
 *   0x05 PING          : (无载荷)
 */
class ServoProtocol : public QObject
{
    Q_OBJECT
public:
    enum Direction {
        DIR_STOP  = 0,
        DIR_DOWN    = 1,
        DIR_UP  = 2,
        DIR_LEFT  = 3,
        DIR_RIGHT = 4
    };

    explicit ServoProtocol(QObject* parent = nullptr);
    ~ServoProtocol() override;

    /// 连接到服务器
    void connectToHost(const QString& ip, quint16 port);
    /// 断开连接
    void disconnectFromHost();
    /// 是否已连接
    bool isConnected() const;

    // --- 业务指令 ---

    /// 舵机移动 (相对方向)
    void servoMove(quint8 servoId, Direction dir, quint8 speed = 80);
    /// 舵机停止
    void servoStop(quint8 servoId);
    /// 舵机绝对角度 (pan 水平, tilt 垂直, 单位 0.01°)
    void servoAbs(quint8 servoId, qint16 pan, qint16 tilt);
    /// 追踪开关
    void setTracking(bool enable);
    /// 流控开关
    void setStream(bool enable, quint16 udpDataPort, quint16 udpVideoPort);
    /// 心跳
    void ping();

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& msg);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError err);

private:
    void sendPacket(quint8 cmd, const QByteArray& payload = QByteArray());
    static quint8 crc8(const QByteArray& data);

    QTcpSocket* m_socket;
};

#endif // SERVO_PROTOCOL_H
