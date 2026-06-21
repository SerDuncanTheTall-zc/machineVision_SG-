cmake_minimum_required(VERSION 3.10)
project(servo_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(servo_server
    servo_server.cpp
)

target_include_directories(servo_server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(Threads REQUIRED)
target_link_libraries(servo_server
    PRIVATE
        Threads::Threads
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(servo_server PRIVATE -Wall -Wextra -O2)
endif()

install(TARGETS servo_server RUNTIME DESTINATION bin)
