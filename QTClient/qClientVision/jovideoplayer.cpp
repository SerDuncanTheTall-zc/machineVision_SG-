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

    // 舵机控制 (D-Pad) — 按下移动，松开停止
    auto btnList = { m_btnUp, m_btnDown, m_btnLeft, m_btnRight };
    for (auto b : btnList) {
        b->setAutoRepeat(true);
        b->setAutoRepeatDelay(200);
        b->setAutoRepeatInterval(80);
        connect(b, &QPushButton::pressed, this, [this, b]() {
            quint8 id = currentServoId();
            if (b == m_btnUp)    m_protocol->servoMove(id, ServoProtocol::DIR_UP);
            if (b == m_btnDown)  m_protocol->servoMove(id, ServoProtocol::DIR_DOWN);
            if (b == m_btnLeft)  m_protocol->servoMove(id, ServoProtocol::DIR_LEFT);
            if (b == m_btnRight) m_protocol->servoMove(id, ServoProtocol::DIR_RIGHT);
        });
        connect(b, &QPushButton::released, this, [this]() {
            m_protocol->servoStop(currentServoId());
        });
    }

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

    // 舵机选择 + 方向键
    auto* servoBox = new QVBoxLayout();

    // 舵机下拉选择
    auto* servoSelectLayout = new QHBoxLayout();
    servoSelectLayout->addWidget(new QLabel("Servo:"));
    m_cmbServo = new QComboBox();
    m_cmbServo->addItem("Servo #1", 1);
    m_cmbServo->addItem("Servo #2", 2);
    m_cmbServo->addItem("Servo #3", 3);
    servoSelectLayout->addWidget(m_cmbServo);
    servoSelectLayout->addStretch();
    servoBox->addLayout(servoSelectLayout);

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
    m_cmbServo->setEnabled(connected);
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
}

void JoVideoPlayer::saveConfig()
{
    QSettings s("JoVision", "RK3576Controller");
    s.setValue("network/ip",        m_editIp->text());
    s.setValue("network/tcp_port",  m_editTcpPort->text().toUInt());
    s.setValue("network/udp_data",  m_editUdpData->text().toUInt());
    s.setValue("network/udp_video", m_editUdpVideo->text().toUInt());
    s.sync();
}

quint8 JoVideoPlayer::currentServoId() const
{
    return static_cast<quint8>(m_cmbServo->currentData().toUInt());
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
