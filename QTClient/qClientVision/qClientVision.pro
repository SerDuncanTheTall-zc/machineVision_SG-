QT += widgets network

CONFIG += c++17

# 1. 自动引入生成的 Protobuf 文件
SOURCES += \
    main.cpp \
    jovideoplayer.cpp \
    gesture_receiver.cpp \
    vcpkg_installed/x64-windows/include/gesture.pb.cc \
    video_canvas.cpp

HEADERS += \
    jovideoplayer.h \
    gesture_receiver.h \
    video_canvas.h \
    gesture.pb.h

# 2. vcpkg 路径配置 (根据你的实际路径调整)
VCPKG_ROOT = C:/Users/jojoz/myCode/vcpkg
VCPKG_INSTALLED = $$PWD/vcpkg_installed/x64-windows

INCLUDEPATH += $$VCPKG_INSTALLED/include

CONFIG(debug, debug|release) {
    # Debug 模式链接 libprotobufd (注意那个 d)
    LIBS += -L$$VCPKG_INSTALLED/debug/lib -llibprotobufd
} else {
    # Release 模式链接 libprotobuf
    LIBS += -L$$VCPKG_INSTALLED/lib -llibprotobuf
}
