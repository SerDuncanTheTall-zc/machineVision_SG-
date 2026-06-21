#ifndef JOVIDEOPLAYER_H
#define JOVIDEOPLAYER_H

#include <QMainWindow>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QSettings>
#include "video_canvas.h"
#include "gesture_receiver.h"
#include "gst_video_receiver.h"
#include "servo_protocol.h"

// 网络配置结构体
struct NetConfig {
    QString serverIp = "192.168.2.73";
    quint16 tcpPort = 9000;
    quint16 udpDataPort = 8888;
    quint16 udpVideoPort = 5000;
    quint8  tiltServoId = 1;   // 上下舵机编号
    quint8  panServoId  = 2;   // 左右舵机编号
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
    void onProtocolConnected();
    void onProtocolDisconnected();
    void onProtocolError(const QString& msg);

    // 指令逻辑
    void onTrackingToggled(bool checked);
    void onRotateClicked();
    void handleGestureData(uint64_t ts, const QList<HandData>& hands);

private:
    void setupUi();
    void updateUiState(bool connected);
    void loadConfig();
    void saveConfig();

    /// 将 ComboBox 切换到指定 data 值
    static void selectComboData(QComboBox* cmb, int data);
    /// 从 ComboBox 读取 servo ID
    static quint8 comboId(const QComboBox* cmb);

    // 核心组件
    VideoCanvas* m_canvas;
    GestureReceiver* m_gestureReceiver;
    GstVideoReceiver* m_videoReceiver;
    ServoProtocol* m_protocol;

    // 当前生效的配置
    NetConfig m_config;

    // UI 元素
    QLineEdit* m_editIp, * m_editTcpPort, * m_editUdpData, * m_editUdpVideo;
    QPushButton* m_btnConnect;
    QPushButton* m_btnSaveConfig;
    QComboBox* m_cmbTiltServo;   // 上下舵机
    QComboBox* m_cmbPanServo;    // 左右舵机
    QPushButton* m_btnUp, * m_btnDown, * m_btnLeft, * m_btnRight;
    QPushButton* m_btnRotate;
    QCheckBox* m_cbTrackingMode;
    QCheckBox* m_cbShowOverlay;
};

#endif
