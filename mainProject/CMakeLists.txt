cmake_minimum_required(VERSION 3.10)
project(machineVision_SG_Server)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------------
# 1. 查找依赖库
# ----------------------------------------------------------------------------

# 查找 GStreamer (推流核心 + app 交互)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)

# 查找 OpenCV (用于某些轻量级预处理/后处理画图)
find_package(OpenCV REQUIRED)

# 查找 Protobuf (数据序列化)
find_package(Protobuf REQUIRED)

# 线程支持 (用于并发处理视频流和 TCP 控制)
find_package(Threads REQUIRED)

# ----------------------------------------------------------------------------
# 2. 硬件加速相关路径 (RK3576 特有)
# ----------------------------------------------------------------------------
# 设置 RKNN 库的头文件路径 (参考之前跑通的路径结构)
# 如果你是系统全局安装的，保留 /usr/include/rknpu2 即可
# 但如果是你下载的 SDK 目录，请修改为实际路径
set(RKNN_API_PATH /usr/include/rknpu2) 
include_directories(${RKNN_API_PATH})

# 如果库在 /usr/lib 或 /usr/local/lib，系统会自动找到
# link_directories(/usr/lib/rknpu2) 

# ----------------------------------------------------------------------------
# 3. 生成 Protobuf 源码
# ----------------------------------------------------------------------------
# 确保你的工程目录下有一个 protocol 文件夹，里面放着 data_packet.proto
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS protocol/data_packet.proto)

# ----------------------------------------------------------------------------
# 4. 包含目录
# ----------------------------------------------------------------------------
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}  # Protobuf 生成的头文件会在这里
    ${GST_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    ${PROTOBUF_INCLUDE_DIRS}
)

# ----------------------------------------------------------------------------
# 5. 定义执行文件
# ----------------------------------------------------------------------------
set(SOURCE_FILES
    main.cpp
    tcp_server.cpp
    servo_manager.cpp
    vision_controller.cpp
    gst_streamer.cpp
    yolo_inference.cpp
    ${PROTO_SRCS}
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# ----------------------------------------------------------------------------
# 6. 链接库 (关键修复区)
# ----------------------------------------------------------------------------
target_link_libraries(${PROJECT_NAME}
    ${GST_LIBRARIES}
    ${OpenCV_LIBS}
    ${PROTOBUF_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    rknnrt          # 修复: 链接 RK3576 的 NPU 运行时库
    rockchip_mpp    # 链接 硬件编解码库
    rga             # 增加: 链接 2D 硬件图形加速库 (用于 Resize 和 Format 转换)
)