#include "jovideoplayer.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    JoVideoPlayer w;
    w.show();
    return QCoreApplication::exec();
}
