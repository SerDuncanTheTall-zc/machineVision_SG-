#include "jovideoplayer.h"
#include <QApplication>
#include <gst/gst.h> // 必须引入

int main(int argc, char *argv[])
{
    // 初始化 GStreamer，这就像是给播放器通电

    //qputenv("GST_DEBUG", "nvh264dec:5,nvcodec:5,basesink:3");
    //qputenv("GST_DEBUG_FILE", "C:/Users/jojoz/myCode/machineVision_SG-/temp/gstreamer_debug.log"); // 输出到文件，防止控制台卡死
    //qputenv("GST_DEBUG_DUMP_DOT_DIR", "C:/Users/jojoz/myCode/machineVision_SG-/temp/");


    gst_init(&argc, &argv);
    qDebug() << "GStreamer version:" << gst_version_string();
    QApplication a(argc, argv);
    JoVideoPlayer w;
    w.show();
    return a.exec();
}