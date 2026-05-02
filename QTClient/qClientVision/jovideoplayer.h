#ifndef JOVIDEOPLAYER_H
#define JOVIDEOPLAYER_H

#include <QMainWindow>

class JoVideoPlayer : public QMainWindow
{
    Q_OBJECT

public:
    explicit JoVideoPlayer(QWidget *parent = nullptr);
    ~JoVideoPlayer() override;
};
#endif // JOVIDEOPLAYER_H
