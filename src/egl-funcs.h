#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#ifdef PLATFORM_BRCM
#include <EGL/eglext_brcm.h>
#endif

typedef EGLDisplay (*PREALEGLGETDISPLAY)(EGLNativeDisplayType);
typedef EGLBoolean (*PREALEGLSWAPBUFFERS)(EGLDisplay, EGLSurface);
typedef EGLSurface (*PREALEGLCREATEWINDOWSURFACE)(EGLDisplay, EGLConfig, EGLNativeWindowType, EGLint const*);
typedef EGLSurface (*PREALEGLCREATEPLATFORMWINDOWSURFACE)(EGLDisplay, EGLConfig, void*, EGLint const*);
typedef EGLBoolean (*PREALEGLDESTROYSURFACE)(EGLDisplay, EGLSurface);
typedef EGLBoolean (*PREALEGLQUERYSURFACE)(EGLDisplay, EGLSurface, EGLint, EGLint*);
typedef EGLBoolean (*PREALEGLMAKECURRENT)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
typedef void (*PREALGLBINDFRAMEBUFFER)(GLenum, GLuint);

struct EGLFuncs
{
    EGLFuncs();
    bool Init();

    PREALEGLGETDISPLAY eglGetDisplay;
    PREALEGLSWAPBUFFERS eglSwapBuffers;
    PREALEGLCREATEWINDOWSURFACE eglCreateWindowSurface;
    PREALEGLCREATEPLATFORMWINDOWSURFACE eglCreatePlatformWindowSurface;
    PREALEGLDESTROYSURFACE  eglDestroySurface;
    PREALEGLQUERYSURFACE eglQuerySurface;
    PREALEGLMAKECURRENT eglMakeCurrent;
    PREALGLBINDFRAMEBUFFER glBindFramebuffer;
    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

private:
    bool inited_;
};