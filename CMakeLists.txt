cmake_minimum_required(VERSION 3.30)
project(display)

find_library(zmq REQUIRED NAMES zmq)

find_package(OpenGL REQUIRED COMPONENTS OpenGL EGL GLES2)

set(CMAKE_CXX_STANDARD 20)

add_executable(display main.cpp)

target_link_libraries(display PRIVATE zmq)
target_link_libraries(display PRIVATE OpenGL EGL GLESv2)