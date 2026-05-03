#ifndef JOVIDEOPLAYER_H
#define JOVIDEOPLAYER_H

#include <QMainWindow>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QTcpSocket>
#include <QLabel>
#include "video_canvas.h"
#include "gesture_receiver.h"
#include "gst_video_receiver.h"

// 网络配置结构体
struct NetConfig {
    QString serverIp = "192.168.2.73";
    quint16 tcpPort = 9000;
    quint16 udpDataPort = 8888;
    quint16 udpVideoPort = 5000;
};

class JoVideoPlayer : public QMainWindow
{
    Q_OBJECT

public:
    explicit JoVideoPlayer(QWidget* parent = nullptr);
    ~JoVideoPlayer() override;

private slots:
    // 连接逻辑
    void onBtnConnectClicked();
    void onTcpConnected();
    void onTcpDisconnected();
    void onTcpError(QAbstractSocket::SocketError error);

    // 指令逻辑
    void sendServoCommand();
    void onTrackingToggled(bool checked);
    void handleGestureData(uint64_t ts, const QList<HandData>& hands);

private:
    void setupUi();
    void updateUiState(bool connected);

    // 核心组件
    VideoCanvas* m_canvas;
    GestureReceiver* m_gestureReceiver;
    GstVideoReceiver* m_videoReceiver;
    QTcpSocket* m_tcpSocket;

    // 当前生效的配置
    NetConfig m_config;

    // UI 元素
    QLineEdit* m_editIp, * m_editTcpPort, * m_editUdpData, * m_editUdpVideo;
    QPushButton* m_btnConnect;
    QPushButton* m_btnUp, * m_btnDown, * m_btnLeft, * m_btnRight;
    QCheckBox* m_cbTrackingMode;
    QCheckBox* m_cbShowOverlay;
};

#endif