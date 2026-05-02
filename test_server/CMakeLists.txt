cmake_minimum_required(VERSION 3.10)
project(GestureTestServer)

# 要求 C++11 或以上版本 (Protobuf 3.21 需要)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. 查找 Protobuf 包
find_package(Protobuf REQUIRED)

# 2. 定义路径 (基于你的 tree 结构)
set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../protoBuf_/protoBuf_RK3576")

# 3. 包含头文件路径
include_directories(
    ${PROTOBUF_INCLUDE_DIRS}
    ${PROTO_DIR}
)

# 4. 定义源文件
# 我们需要包含生成的 .pb.cc 文件
set(SOURCES 
    main.cpp 
    "${PROTO_DIR}/gesture.pb.cc"
)

# 5. 生成可执行程序
add_executable(gesture_test_server ${SOURCES})

# 6. 链接库 (Protobuf 和 线程库)
target_link_libraries(gesture_test_server 
    ${PROTOBUF_LIBRARIES}
    pthread
)

# 打印信息方便调试
message(STATUS "Protobuf 库路径: ${PROTOBUF_LIBRARIES}")
message(STATUS "Protobuf 头文件: ${PROTOBUF_INCLUDE_DIRS}")