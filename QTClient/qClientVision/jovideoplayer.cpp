#include "jovideoplayer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QIntValidator>

JoVideoPlayer::JoVideoPlayer(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();

    m_tcpSocket = new QTcpSocket(this);
    m_gestureReceiver = new GestureReceiver(this);
    m_videoReceiver = new GstVideoReceiver(this);

    // TCP 状态监听
    connect(m_btnConnect, &QPushButton::clicked, this, &JoVideoPlayer::onBtnConnectClicked);
    connect(m_tcpSocket, &QTcpSocket::connected, this, &JoVideoPlayer::onTcpConnected);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &JoVideoPlayer::onTcpDisconnected);
    //connect(m_tcpSocket, &QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &JoVideoPlayer::onTcpError);
    connect(m_tcpSocket, &QTcpSocket::errorOccurred, this, &JoVideoPlayer::onTcpError);
    // 数据流链路
    connect(m_gestureReceiver, &GestureReceiver::dataReceived, this, &JoVideoPlayer::handleGestureData);
    connect(m_videoReceiver, &GstVideoReceiver::frameReady, m_canvas, &VideoCanvas::updateVideoFrame);

    // UI 交互
    connect(m_cbShowOverlay, &QCheckBox::toggled, m_canvas, &VideoCanvas::setDrawOverlay);
    connect(m_cbTrackingMode, &QCheckBox::toggled, this, &JoVideoPlayer::onTrackingToggled);

    // 舵机控制 (D-Pad)
    auto btnList = { m_btnUp, m_btnDown, m_btnLeft, m_btnRight };
    for (auto b : btnList) connect(b, &QPushButton::clicked, this, &JoVideoPlayer::sendServoCommand);

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
    bottomLayout->addLayout(visionBox);

    bottomLayout->addSpacing(40);

    // 舵机方向键
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
    bottomLayout->addLayout(dpad);

    bottomLayout->addStretch();
    mainLayout->addLayout(bottomLayout);

    this->setCentralWidget(centralWidget);
}

void JoVideoPlayer::onBtnConnectClicked()
{
    if (m_tcpSocket->state() == QAbstractSocket::UnconnectedState) {
        // 更新当前配置
        m_config.serverIp = m_editIp->text();
        m_config.tcpPort = m_editTcpPort->text().toUShort();
        m_config.udpDataPort = m_editUdpData->text().toUShort();
        m_config.udpVideoPort = m_editUdpVideo->text().toUShort();

        m_tcpSocket->connectToHost(m_config.serverIp, m_config.tcpPort);
        m_btnConnect->setText("Connecting...");
    }
    else {
        m_tcpSocket->disconnectFromHost();
    }
}

void JoVideoPlayer::onTcpConnected()
{
    updateUiState(true);

    // 只有 TCP 连上了，才根据配置打开 UDP 接收器
    m_gestureReceiver->start(m_config.udpDataPort);
    m_videoReceiver->start(m_config.udpVideoPort);

    // 通知板子开启流推送
    m_tcpSocket->write("CMD_START_STREAM\n");
}

void JoVideoPlayer::onTcpDisconnected()
{
    updateUiState(false);
    m_gestureReceiver->stop();
    m_videoReceiver->stop();
}

void JoVideoPlayer::onTcpError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    QMessageBox::warning(this, "Connection Error", m_tcpSocket->errorString());
    updateUiState(false);
}

void JoVideoPlayer::updateUiState(bool connected)
{
    m_btnConnect->setText(connected ? "Disconnect" : "Connect");
    m_editIp->setEnabled(!connected);
    m_editTcpPort->setEnabled(!connected);
    m_editUdpData->setEnabled(!connected);
    m_editUdpVideo->setEnabled(!connected);

    m_btnUp->setEnabled(connected);
    m_btnDown->setEnabled(connected);
    m_btnLeft->setEnabled(connected);
    m_btnRight->setEnabled(connected);
    m_cbTrackingMode->setEnabled(connected);
}

void JoVideoPlayer::sendServoCommand()
{
    if (!m_tcpSocket->isOpen()) return;
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (btn == m_btnUp)    m_tcpSocket->write("SERVO_UP\n");
    if (btn == m_btnDown)  m_tcpSocket->write("SERVO_DOWN\n");
    if (btn == m_btnLeft)  m_tcpSocket->write("SERVO_LEFT\n");
    if (btn == m_btnRight) m_tcpSocket->write("SERVO_RIGHT\n");
}

void JoVideoPlayer::onTrackingToggled(bool checked)
{
    if (m_tcpSocket->isOpen()) {
        m_tcpSocket->write(checked ? "TRACKING_ON\n" : "TRACKING_OFF\n");
    }
}

void JoVideoPlayer::handleGestureData(uint64_t ts, const QList<HandData>& hands)
{
    Q_UNUSED(ts);
    m_canvas->updateOverlay(hands);
}