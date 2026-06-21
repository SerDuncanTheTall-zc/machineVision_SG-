cmake_minimum_required(VERSION 3.10)
project(servo_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- GStreamer 查找 ---
# 板端一般已安装 gstreamer，默认路径为 /usr
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED
    gstreamer-1.0
    gstreamer-video-1.0
)

add_executable(servo_server
    servo_server.cpp
    gst_streamer.cpp
)

target_include_directories(servo_server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${GST_INCLUDE_DIRS}
)

target_compile_options(servo_server
    PRIVATE
        ${GST_CFLAGS_OTHER}
)

find_package(Threads REQUIRED)
target_link_libraries(servo_server
    PRIVATE
        Threads::Threads
        ${GST_LIBRARIES}
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(servo_server PRIVATE -Wall -Wextra -O2)
endif()

install(TARGETS servo_server RUNTIME DESTINATION bin)
