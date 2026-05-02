#include "jovideoplayer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

JoVideoPlayer::JoVideoPlayer(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();

    m_receiver = new GestureReceiver(this);

    // 绑定信号与槽
    connect(m_btnStart, &QPushButton::clicked, this, &JoVideoPlayer::onBtnStartClicked);
    connect(m_cbShowOverlay, &QCheckBox::toggled, m_canvas, &VideoCanvas::setDrawOverlay);
    connect(m_receiver, &GestureReceiver::dataReceived, this, &JoVideoPlayer::handleGestureData);
}

JoVideoPlayer::~JoVideoPlayer() = default;

void JoVideoPlayer::setupUi()
{
    this->setWindowTitle("Machine Vision Client - RK3576");
    this->resize(800, 600);

    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    // 1. 视频画布区域，设置拉伸因子为1，让它占据大部分空间
    m_canvas = new VideoCanvas(this);
    m_canvas->setMinimumSize(640, 480);
    mainLayout->addWidget(m_canvas, 1);

    // 2. 底部控制栏
    auto *controlLayout = new QHBoxLayout();

    m_btnStart = new QPushButton("Start Receiving", this);
    m_btnStart->setMinimumHeight(40);

    m_cbShowOverlay = new QCheckBox("Show Overlay (Bbox)", this);
    m_cbShowOverlay->setChecked(true); // 默认开启

    controlLayout->addWidget(m_btnStart);
    controlLayout->addSpacing(20);
    controlLayout->addWidget(m_cbShowOverlay);
    controlLayout->addStretch(); // 把按钮挤到左边

    mainLayout->addLayout(controlLayout);

    this->setCentralWidget(centralWidget);
}

void JoVideoPlayer::onBtnStartClicked()
{
    if (m_receiver->isRunning()) {
        m_receiver->stop();
        m_btnStart->setText("Start Receiving");
        // 清空画面上的框
        m_canvas->updateOverlay(QList<HandData>());
    } else {
        if (m_receiver->start(PORT)) {
            m_btnStart->setText("Stop");
        }
    }
}

void JoVideoPlayer::handleGestureData(uint64_t ts, const QList<HandData>& hands)
{
    Q_UNUSED(ts); // 后续如果做音画同步，会用 ts 匹配 QImage 的 PTS

    // 直接扔给画布渲染
    m_canvas->updateOverlay(hands);
}
