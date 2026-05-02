#ifndef JOVIDEOPLAYER_H
#define JOVIDEOPLAYER_H

#include <QMainWindow>
#include <QPushButton>
#include <QCheckBox>
#include "video_canvas.h"
#include "gesture_receiver.h"

class JoVideoPlayer : public QMainWindow
{
    Q_OBJECT

public:
    explicit JoVideoPlayer(QWidget *parent = nullptr);
    ~JoVideoPlayer() override;

private slots:
    void onBtnStartClicked();
    void handleGestureData(uint64_t ts, const QList<HandData>& hands);

private:
    void setupUi();

    VideoCanvas *m_canvas;
    GestureReceiver *m_receiver;

    QPushButton *m_btnStart;
    QCheckBox *m_cbShowOverlay;

    // 监听端口，目前写死 8888
    const quint16 PORT = 8888;
};

#endif // JOVIDEOPLAYER_H
