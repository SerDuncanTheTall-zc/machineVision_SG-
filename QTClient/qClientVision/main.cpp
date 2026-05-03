#include "jovideoplayer.h"
#include <QApplication>
#include <gst/gst.h> // 必须引入

int main(int argc, char *argv[])
{
    // 初始化 GStreamer，这就像是给播放器通电
    gst_init(&argc, &argv);
    qDebug() << "GStreamer version:" << gst_version_string();
    QApplication a(argc, argv);
    JoVideoPlayer w;
    w.show();
    return a.exec();
}