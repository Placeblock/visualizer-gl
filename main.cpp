#include <chrono>
#include <stdexcept>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <zmq.h>
#include <csignal>
#include <fstream>
#include <iostream>
#include <sstream> //std::stringstream
#include <thread>

#include "audio.hpp"
#include <glad/gl.h>

constexpr int WIDTH = 128;
constexpr int HEIGHT = 32;

void checkGLError(const char* msg) {
    if (const GLenum err = glGetError(); err != GL_NO_ERROR) {
        fprintf(stderr, "GL Error after %s: %x\n", msg, err);
    }
}

EGLDisplay display;
EGLContext context;
EGLSurface surface;

void *zmqContext;
void *sender;

GLint timeAttributeLocation;
GLint resolutionAttributeLocation;
GLint amplitudeAttributeLocation;
GLuint fftSSBO;
GLuint logFftSSBO;

Audio audio;
std::thread audioThread;
    

void destroy() {
    zmq_close(sender);
    zmq_ctx_destroy(zmqContext);
    
    eglDestroySurface(display, surface);
    eglDestroyContext(display, context);
    eglTerminate(display);
    
    printf("Closed.\n");
}

void intHandler(int _) {
    destroy();
    audio.running = false;
}

void initZMQ() {
    zmqContext = zmq_ctx_new();
    sender = zmq_socket(zmqContext, ZMQ_REQ);
    const int res = zmq_connect(sender, "tcp://matrix.kwsnet:5555");
    printf("ZeroMQ: %d\n", res);
}

void initEGL() {
    if (eglBindAPI(EGL_OPENGL_API) != EGL_TRUE) {
        throw std::runtime_error("Failed to bind OpenGL API");
    }

    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if(eglInitialize(display, nullptr, nullptr) != EGL_TRUE){
        switch(eglGetError()){
            case EGL_BAD_DISPLAY:
                throw std::runtime_error("Failed to initialize EGL Display: EGL_BAD_DISPLAY");
            case EGL_NOT_INITIALIZED:
                throw std::runtime_error("Failed to initialize EGL Display: EGL_NOT_INITIALIZED");
            default:
                throw std::runtime_error("Failed to initialize EGL Display: unknown error");
        }
    }

    EGLConfig eglConfig;
    EGLint numConfigs;
    const EGLint eglConfigAttributes[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 6, EGL_GREEN_SIZE, 6, EGL_BLUE_SIZE, 6, EGL_NONE
    };
    if(eglChooseConfig(display, eglConfigAttributes, &eglConfig, 1, &numConfigs) != EGL_TRUE){
        switch(eglGetError()){
            case EGL_BAD_DISPLAY:
                throw std::runtime_error("Failed to configure EGL Display: EGL_BAD_DISPLAY");
            case EGL_BAD_ATTRIBUTE:
                throw std::runtime_error("Failed to configure EGL Display: EGL_BAD_ATTRIBUTE");
            case EGL_NOT_INITIALIZED:
                throw std::runtime_error("Failed to configure EGL Display: EGL_NOT_INITIALIZED");
            case EGL_BAD_PARAMETER:
                throw std::runtime_error("Failed to configure EGL Display: EGL_BAD_PARAMETER");
            default:
                throw std::runtime_error("Failed to configure EGL Display: unknown error");
        }
    }

    context = eglCreateContext(display, eglConfig, EGL_NO_CONTEXT, nullptr);
    if (context == EGL_NO_CONTEXT) {
        eglTerminate(display);
        throw std::runtime_error("Failed to create EGL Context");
    }

    constexpr EGLint eglSurfaceAttributes[] = {
        EGL_WIDTH, WIDTH, EGL_HEIGHT, HEIGHT, EGL_NONE
    };
    surface = eglCreatePbufferSurface(display, eglConfig, eglSurfaceAttributes);
    if (surface == EGL_NO_SURFACE) {
        eglDestroyContext(display, context);
        eglTerminate(display);
        throw std::runtime_error("Failed to create EGL surface");
    }

    if (eglMakeCurrent(display, surface, surface, context) != EGL_TRUE) {
        throw std::runtime_error("Failed to make EGL context current");
    }
}

void initOpenGL() {
    gladLoadGL(eglGetProcAddress); 
    glViewport(0, 0, WIDTH, HEIGHT);
    
    float vertices[] = {-1, -1, -1, 1, 1, 1, -1, -1, 1, -1, 1, 1};
    unsigned int VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    
    std::ifstream vertexShaderFile("shader.vert");
    if (!vertexShaderFile.is_open()) {
        throw std::runtime_error("Could not open shader.vert\n");
    }
    std::stringstream vertexShaderStream;
    vertexShaderStream << vertexShaderFile.rdbuf();
    std::string vertexShaderSource = vertexShaderStream.str();
    const char *rawVertexShaderSource = vertexShaderSource.c_str();
    
    std::ifstream fragmentShaderFile("shader.frag");
    if (!fragmentShaderFile.is_open()) {
        throw std::runtime_error("Could not open shader.frag\n");
    }
    std::stringstream fragmentShaderStream;
    fragmentShaderStream << fragmentShaderFile.rdbuf();
    std::string fragmentShaderSource = fragmentShaderStream.str();
    const char *rawFragmentShaderSource = fragmentShaderSource.c_str();

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &rawVertexShaderSource, nullptr);
    glCompileShader(vertexShader);
    int success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cout << "Compiler Error: " << infoLog << std::endl;
        throw std::runtime_error("Failed to compile vertex shader\n");
    }
    
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &rawFragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cout << "Compiler Error: " << infoLog << std::endl;
        throw std::runtime_error("Failed to compile fragment shader\n");
    }
    
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cout << "Link Error: " << infoLog << std::endl;
        throw std::runtime_error("Failed to link shaders\n");
    }

    glUseProgram(program);
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    timeAttributeLocation = glGetUniformLocation(program, "time");
    resolutionAttributeLocation = glGetUniformLocation(program, "res");
    amplitudeAttributeLocation = glGetUniformLocation(program, "amplitude");
    
    glGenBuffers(1, &fftSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, fftSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, fftSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    glGenBuffers(1, &logFftSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, logFftSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, logFftSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// TIP To <b>Run</b> code, press <shortcut actionId="Run"/> or
// click the <icon src="AllIcons.Actions.Execute"/> icon in the gutter.
int main() {
    audio.init();
    audioThread = std::thread(&Audio::start, audio);
    audioThread.detach();
    
    signal(SIGINT, intHandler);

    initZMQ();
    initEGL();
    initOpenGL();
    
    std::chrono::high_resolution_clock::time_point lastFrameTime = std::chrono::high_resolution_clock::now();
    while (true) {
        auto currentFrameTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentFrameTime - lastFrameTime);
        
        glClearColor(1.0, 0.0, 0.0, 1.0); // Red background
        glClear(GL_COLOR_BUFFER_BIT);

        glUniform1f(timeAttributeLocation, static_cast<float>(elapsed.count()));
        glUniform2f(resolutionAttributeLocation, WIDTH, HEIGHT);
        glUniform1f(amplitudeAttributeLocation, audio.amplitude);

        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, fftSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, FFW_BANDS*sizeof(fftw_complex), *audio.result, GL_DYNAMIC_DRAW); 
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, logFftSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, audio.logResult->size()*sizeof(float), audio.logResult->data(), GL_DYNAMIC_DRAW); 
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        unsigned char pixels[WIDTH * HEIGHT * 3];
        glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, pixels);

        zmq_send(sender, pixels, sizeof(pixels), 0);
        zmq_recv(sender, nullptr, 0, 0);
    }
}

// TIP See CLion help at <a
// href="https://www.jetbrains.com/help/clion/">jetbrains.com/help/clion/</a>.
//  Also, you can try interactive lessons for CLion by selecting
//  'Help | Learn IDE Features' from the main menu.