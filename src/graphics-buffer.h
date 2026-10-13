#pragma once

#include "egl-funcs.h"

class PlaneControllerClient;

class GraphicsBuffer
{
public:
    GraphicsBuffer(PlaneControllerClient* client, EGLDisplay eglDisplay);
    ~GraphicsBuffer();
    
    bool Create();
    bool Commit();
    bool WaitOnRelease();
    bool Destroy();
    bool Load();
    GLuint Fbo() { return fbo_; }
private:

    PlaneControllerClient* client_;
    EGLDisplay eglDisplay_;
    uint32_t bufferId_;
    uint32_t width_;
    uint32_t height_;
    uint32_t stride_;
    uint32_t offset_;
    uint32_t format_;
    uint32_t fd_;
    bool committed_;
    EGLImageKHR image_;
    GLuint texture_;
    GLuint fbo_;
};