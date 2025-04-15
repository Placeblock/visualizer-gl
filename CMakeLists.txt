cmake_minimum_required(VERSION 3.30)
project(display)

find_library(zmq REQUIRED NAMES zmq)
find_library(portaudio REQUIRED NAMES portaudio)
find_library(fftw3f REQUIRED NAMES fftw3)

include_directories("include")

find_package(OpenGL REQUIRED COMPONENTS OpenGL EGL GLES2)

set(CMAKE_CXX_STANDARD 20)

add_executable(display main.cpp audio.cpp gl.c)

target_link_libraries(display PRIVATE zmq OpenGL EGL GLESv2 portaudio fftw3f)