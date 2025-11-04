cmake_minimum_required(VERSION 3.10)
project(one_motor LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(one_motor
backend/main.cpp
backend/HttpServer.cpp
backend/MotorController.cpp
backend/SerialPort.cpp
)


# pthread for std::thread and sockets on Linux
find_package(Threads REQUIRED)


if(UNIX)
target_link_libraries(one_motor PRIVATE Threads::Threads)
endif()


# Enable warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
target_compile_options(one_motor PRIVATE -Wall -Wextra -Wpedantic)
endif()