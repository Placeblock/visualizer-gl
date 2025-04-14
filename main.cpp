#include <exception>
#include <stdexcept>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <zmq.h>

constexpr int WIDTH = 128;
constexpr int HEIGHT = 32;

void checkGLError(const char* msg) {
    if (const GLenum err = glGetError(); err != GL_NO_ERROR) {
        fprintf(stderr, "GL Error after %s: %x\n", msg, err);
    }
}

// TIP To <b>Run</b> code, press <shortcut actionId="Run"/> or
// click the <icon src="AllIcons.Actions.Execute"/> icon in the gutter.
int main() {
    if (eglBindAPI(EGL_OPENGL_API) != EGL_TRUE) {
        throw std::runtime_error("Failed to bind OpenGL API");
    };

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
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

    const EGLContext context = eglCreateContext(display, eglConfig, EGL_NO_CONTEXT, nullptr);
    if (context == EGL_NO_CONTEXT) {
        throw std::runtime_error("Failed to create EGL Context");
    }

    constexpr EGLint eglSurfaceAttributes[] = {
        EGL_WIDTH, WIDTH, EGL_HEIGHT, HEIGHT, EGL_NONE
    };
    const EGLSurface surface = eglCreatePbufferSurface(display, eglConfig, eglSurfaceAttributes);
    if (surface == EGL_NO_SURFACE) {
        throw std::runtime_error("Failed to create EGL surface");
    }

    if (eglMakeCurrent(display, surface, surface, context) != EGL_TRUE) {
        throw std::runtime_error("Failed to make EGL context current");
    }

    eglDestroySurface(display, surface);
    eglDestroyContext(display, context);
    eglTerminate(display);

    return 0;
}

// TIP See CLion help at <a
// href="https://www.jetbrains.com/help/clion/">jetbrains.com/help/clion/</a>.
//  Also, you can try interactive lessons for CLion by selecting
//  'Help | Learn IDE Features' from the main menu.