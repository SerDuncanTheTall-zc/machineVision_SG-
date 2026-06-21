#include "jovideoplayer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QIntValidator>
#include <QGroupBox>

JoVideoPlayer::JoVideoPlayer(QWidget* parent)
    : QMainWindow(parent)
{
    loadConfig();
    setupUi();

    // 回填配置中保存的舵机选择
    selectComboData(m_cmbTiltServo, m_config.tiltServoId);
    selectComboData(m_cmbPanServo,  m_config.panServoId);

    m_protocol = new ServoProtocol(this);
    m_gestureReceiver = new GestureReceiver(this);
    m_videoReceiver = new GstVideoReceiver(this);

    // 协议层信号 → UI 层
    connect(m_protocol, &ServoProtocol::connected,    this, &JoVideoPlayer::onProtocolConnected);
    connect(m_protocol, &ServoProtocol::disconnected, this, &JoVideoPlayer::onProtocolDisconnected);
    connect(m_protocol, &ServoProtocol::errorOccurred,this, &JoVideoPlayer::onProtocolError);

    // 连接按钮
    connect(m_btnConnect, &QPushButton::clicked, this, &JoVideoPlayer::onBtnConnectClicked);
    connect(m_btnSaveConfig, &QPushButton::clicked, this, &JoVideoPlayer::saveConfig);

    // 数据流链路
    connect(m_gestureReceiver, &GestureReceiver::dataReceived, this, &JoVideoPlayer::handleGestureData);
    connect(m_videoReceiver, &GstVideoReceiver::frameReady, m_canvas, &VideoCanvas::updateVideoFrame);

    // UI 交互
    connect(m_cbShowOverlay, &QCheckBox::toggled, m_canvas, &VideoCanvas::setDrawOverlay);
    connect(m_cbTrackingMode, &QCheckBox::toggled, this, &JoVideoPlayer::onTrackingToggled);
    connect(m_btnRotate, &QPushButton::clicked, this, &JoVideoPlayer::onRotateClicked);

    // 舵机控制 (D-Pad)
    //   上/下 → Tilt 舵机,  左/右 → Pan 舵机
    auto setupDpad = [this](QPushButton* btn, ServoProtocol::Direction dir,
                            const QComboBox* cmb, const QComboBox* stopCmb) {
        btn->setAutoRepeat(true);
        btn->setAutoRepeatDelay(200);
        btn->setAutoRepeatInterval(80);
        connect(btn, &QPushButton::pressed, this, [this, cmb, dir]() {
            m_protocol->servoMove(comboId(cmb), dir);
        });
        connect(btn, &QPushButton::released, this, [this, stopCmb]() {
            m_protocol->servoStop(comboId(stopCmb));
        });
    };

    setupDpad(m_btnUp,    ServoProtocol::DIR_UP,    m_cmbTiltServo, m_cmbTiltServo);
    setupDpad(m_btnDown,  ServoProtocol::DIR_DOWN,  m_cmbTiltServo, m_cmbTiltServo);
    setupDpad(m_btnLeft,  ServoProtocol::DIR_LEFT,  m_cmbPanServo,  m_cmbPanServo);
    setupDpad(m_btnRight, ServoProtocol::DIR_RIGHT, m_cmbPanServo,  m_cmbPanServo);

    updateUiState(false);
}

JoVideoPlayer::~JoVideoPlayer() = default;

void JoVideoPlayer::setupUi()
{
    this->setWindowTitle("JoVision - RK3576 Master Controller");
    this->resize(1100, 800);

    auto* centralWidget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(centralWidget);

    // --- 1. 设置区域 (放在顶部) ---
    auto* settingsLayout = new QHBoxLayout();

    m_editIp = new QLineEdit(m_config.serverIp);
    m_editTcpPort = new QLineEdit(QString::number(m_config.tcpPort));
    m_editUdpData = new QLineEdit(QString::number(m_config.udpDataPort));
    m_editUdpVideo = new QLineEdit(QString::number(m_config.udpVideoPort));

    // 限制只能输入数字
    auto* val = new QIntValidator(1, 65535, this);
    m_editTcpPort->setValidator(val);
    m_editUdpData->setValidator(val);
    m_editUdpVideo->setValidator(val);

    settingsLayout->addWidget(new QLabel("IP:")); settingsLayout->addWidget(m_editIp);
    settingsLayout->addWidget(new QLabel("TCP:")); settingsLayout->addWidget(m_editTcpPort);
    settingsLayout->addWidget(new QLabel("UDP-Data:")); settingsLayout->addWidget(m_editUdpData);
    settingsLayout->addWidget(new QLabel("UDP-Video:")); settingsLayout->addWidget(m_editUdpVideo);

    m_btnConnect = new QPushButton("Connect");
    m_btnConnect->setMinimumWidth(100);
    settingsLayout->addWidget(m_btnConnect);

    m_btnSaveConfig = new QPushButton("Save");
    m_btnSaveConfig->setMinimumWidth(60);
    settingsLayout->addWidget(m_btnSaveConfig);

    mainLayout->addLayout(settingsLayout);

    // --- 2. 视频画布 ---
    m_canvas = new VideoCanvas(this);
    mainLayout->addWidget(m_canvas, 1);

    // --- 3. 底部控制区 ---
    auto* bottomLayout = new QHBoxLayout();

    // 视觉开关
    auto* visionBox = new QVBoxLayout();
    m_cbShowOverlay = new QCheckBox("Draw Bbox");
    m_cbShowOverlay->setChecked(true);
    m_cbTrackingMode = new QCheckBox("AI Tracking Mode");
    visionBox->addWidget(m_cbShowOverlay);
    visionBox->addWidget(m_cbTrackingMode);

    m_btnRotate = new QPushButton("Rotate 90°");
    visionBox->addWidget(m_btnRotate);

    bottomLayout->addLayout(visionBox);

    bottomLayout->addSpacing(40);

    // 舵机分配 + 方向键
    auto* servoBox = new QVBoxLayout();

    // 舵机下拉选择：Tilt(上下) + Pan(左右)
    auto makeServoCombo = []() {
        auto* cmb = new QComboBox();
        cmb->addItem("Servo #1", 1);
        cmb->addItem("Servo #2", 2);
        cmb->addItem("Servo #3", 3);
        return cmb;
    };

    auto* servoAssignLayout = new QHBoxLayout();
    servoAssignLayout->addWidget(new QLabel("Tilt(UD):"));
    m_cmbTiltServo = makeServoCombo();
    servoAssignLayout->addWidget(m_cmbTiltServo);
    servoAssignLayout->addSpacing(20);
    servoAssignLayout->addWidget(new QLabel("Pan(LR):"));
    m_cmbPanServo = makeServoCombo();
    servoAssignLayout->addWidget(m_cmbPanServo);
    servoAssignLayout->addStretch();
    servoBox->addLayout(servoAssignLayout);

    // 方向键
    auto* dpad = new QGridLayout();
    m_btnUp = new QPushButton("UP");
    m_btnDown = new QPushButton("DOWN");
    m_btnLeft = new QPushButton("LEFT");
    m_btnRight = new QPushButton("RIGHT");
    QString style = "QPushButton { min-width: 60px; min-height: 40px; font-weight: bold; }";
    m_btnUp->setStyleSheet(style); m_btnDown->setStyleSheet(style);
    m_btnLeft->setStyleSheet(style); m_btnRight->setStyleSheet(style);

    dpad->addWidget(m_btnUp, 0, 1);
    dpad->addWidget(m_btnLeft, 1, 0);
    dpad->addWidget(m_btnRight, 1, 2);
    dpad->addWidget(m_btnDown, 2, 1);
    servoBox->addLayout(dpad);

    bottomLayout->addLayout(servoBox);

    bottomLayout->addStretch();
    mainLayout->addLayout(bottomLayout);

    this->setCentralWidget(centralWidget);
}

void JoVideoPlayer::onBtnConnectClicked()
{
    if (!m_protocol->isConnected()) {
        m_config.serverIp = m_editIp->text();
        m_config.tcpPort = m_editTcpPort->text().toUShort();
        m_config.udpDataPort = m_editUdpData->text().toUShort();
        m_config.udpVideoPort = m_editUdpVideo->text().toUShort();

        m_protocol->connectToHost(m_config.serverIp, m_config.tcpPort);
        m_btnConnect->setText("Connecting...");
    }
    else {
        m_protocol->disconnectFromHost();
    }
}

void JoVideoPlayer::onProtocolConnected()
{
    updateUiState(true);

    m_gestureReceiver->start(m_config.udpDataPort);
    m_videoReceiver->start(m_config.udpVideoPort);

    // 通过协议通知板子开启流推送
    m_protocol->setStream(true, m_config.udpDataPort, m_config.udpVideoPort);
}

void JoVideoPlayer::onProtocolDisconnected()
{
    updateUiState(false);
    m_gestureReceiver->stop();
    m_videoReceiver->stop();
}

void JoVideoPlayer::onProtocolError(const QString& msg)
{
    QMessageBox::warning(this, "Connection Error", msg);
    updateUiState(false);
}

void JoVideoPlayer::updateUiState(bool connected)
{
    m_btnConnect->setText(connected ? "Disconnect" : "Connect");
    m_editIp->setEnabled(!connected);
    m_editTcpPort->setEnabled(!connected);
    m_editUdpData->setEnabled(!connected);
    m_editUdpVideo->setEnabled(!connected);

    m_btnSaveConfig->setEnabled(!connected);
    m_cmbTiltServo->setEnabled(connected);
    m_cmbPanServo->setEnabled(connected);
    m_btnRotate->setEnabled(connected);
    m_btnUp->setEnabled(connected);
    m_btnDown->setEnabled(connected);
    m_btnLeft->setEnabled(connected);
    m_btnRight->setEnabled(connected);
    m_cbTrackingMode->setEnabled(connected);
}

void JoVideoPlayer::loadConfig()
{
    QSettings s("JoVision", "RK3576Controller");
    m_config.serverIp     = s.value("network/ip",         m_config.serverIp).toString();
    m_config.tcpPort      = s.value("network/tcp_port",   m_config.tcpPort).toUInt();
    m_config.udpDataPort  = s.value("network/udp_data",   m_config.udpDataPort).toUInt();
    m_config.udpVideoPort = s.value("network/udp_video",  m_config.udpVideoPort).toUInt();
    m_config.tiltServoId  = static_cast<quint8>(s.value("servo/tilt_id", m_config.tiltServoId).toUInt());
    m_config.panServoId   = static_cast<quint8>(s.value("servo/pan_id",  m_config.panServoId).toUInt());
}

void JoVideoPlayer::saveConfig()
{
    QSettings s("JoVision", "RK3576Controller");
    s.setValue("network/ip",        m_editIp->text());
    s.setValue("network/tcp_port",  m_editTcpPort->text().toUInt());
    s.setValue("network/udp_data",  m_editUdpData->text().toUInt());
    s.setValue("network/udp_video", m_editUdpVideo->text().toUInt());
    s.setValue("servo/tilt_id",     comboId(m_cmbTiltServo));
    s.setValue("servo/pan_id",      comboId(m_cmbPanServo));
    s.sync();
}

quint8 JoVideoPlayer::comboId(const QComboBox* cmb)
{
    return static_cast<quint8>(cmb->currentData().toUInt());
}

void JoVideoPlayer::selectComboData(QComboBox* cmb, int data)
{
    int idx = cmb->findData(data);
    if (idx >= 0) {
        cmb->setCurrentIndex(idx);
    }
}

void JoVideoPlayer::onTrackingToggled(bool checked)
{
    m_protocol->setTracking(checked);
}

void JoVideoPlayer::onRotateClicked()
{
    int next = (m_videoReceiver->rotation() + 90) % 360;
    m_videoReceiver->setRotation(next);
}

void JoVideoPlayer::handleGestureData(uint64_t ts, const QList<HandData>& hands)
{
    Q_UNUSED(ts);
    m_canvas->updateOverlay(hands);
}
