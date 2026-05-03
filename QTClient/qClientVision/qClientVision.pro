QT += widgets network

CONFIG += c++17
GSTREAMER_ROOT = "C:/gstreamer/1.0/msvc_x86_64"
# 1. 自动引入生成的 Protobuf 文件
SOURCES += \
    gst_video_receiver.cpp \
    main.cpp \
    jovideoplayer.cpp \
    gesture_receiver.cpp \
    vcpkg_installed/x64-windows/include/gesture.pb.cc \
    video_canvas.cpp

HEADERS += \
    gst_video_receiver.h \
    jovideoplayer.h \
    gesture_receiver.h \
    video_canvas.h \
    gesture.pb.h

# 2. vcpkg 路径配置 (根据你的实际路径调整)
VCPKG_ROOT = C:/Users/jojoz/myCode/vcpkg
VCPKG_INSTALLED = $$PWD/vcpkg_installed/x64-windows

INCLUDEPATH += $$VCPKG_INSTALLED/include
INCLUDEPATH += $$GSTREAMER_ROOT/include \
               $$GSTREAMER_ROOT/include\gstreamer-1.0 \
               $$GSTREAMER_ROOT/include/glib-2.0 \
               $$GSTREAMER_ROOT/lib/glib-2.0/include

CONFIG(debug, debug|release) {
    # Debug 模式链接 libprotobufd (注意那个 d)
    LIBS += -L$$VCPKG_INSTALLED/debug/lib -llibprotobufd
} else {
    # Release 模式链接 libprotobuf
    LIBS += -L$$VCPKG_INSTALLED/lib -llibprotobuf
}

LIBS += -L$$GSTREAMER_ROOT/lib \
        -lgstreamer-1.0 \
        -lgobject-2.0 \
        -lglib-2.0 \
        -lgstapp-1.0 \
        -lgstvideo-1.0