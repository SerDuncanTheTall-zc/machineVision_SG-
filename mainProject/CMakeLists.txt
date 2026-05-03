cmake_minimum_required(VERSION 3.10)
project(machineVision_SG_Server)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------------
# 1. 查找依赖库
# ----------------------------------------------------------------------------

# 查找 GStreamer (推流核心)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)

# 查找 OpenCV (图像预处理)
find_package(OpenCV REQUIRED)

# 查找 Protobuf (数据序列化)
find_package(Protobuf REQUIRED)

# 线程支持
find_package(Threads REQUIRED)

# ----------------------------------------------------------------------------
# 2. 硬件加速相关路径 (RK3576 特有)
# ----------------------------------------------------------------------------
# 假设 RKNPU2 和 MPP 库安装在标准路径或指定路径
include_directories(/usr/include/rknpu2)
link_directories(/usr/lib/rknpu2)

# ----------------------------------------------------------------------------
# 3. 生成 Protobuf 源码
# ----------------------------------------------------------------------------
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS protocol/data_packet.proto)

# ----------------------------------------------------------------------------
# 4. 包含目录
# ----------------------------------------------------------------------------
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
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
    # 建议增加的推流和推理模块
    gst_streamer.cpp
    yolo_inference.cpp
    ${PROTO_SRCS}
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# ----------------------------------------------------------------------------
# 6. 链接库
# ----------------------------------------------------------------------------
target_link_libraries(${PROJECT_NAME}
    ${GST_LIBRARIES}
    ${OpenCV_LIBS}
    ${PROTOBUF_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    rknn_api       # RKNPU2 核心库
    rockchip_mpp   # 硬件编解码库
)