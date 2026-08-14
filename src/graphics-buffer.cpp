#include "graphics-buffer.h"
#include "egl-funcs.h"
#include "logger.h"
#include "plane-controller-client.h"

#define INVALID_ID (-1)

extern int DISPLAY_WIDTH;
extern int DISPLAY_HEIGHT;

GraphicsBuffer::GraphicsBuffer(PlaneControllerClient* client_, EGLFuncs* eglFuncs, EGLDisplay eglDisplay)
: client_(client_)
, eglFuncs_(eglFuncs)
, eglDisplay_(eglDisplay)
, bufferId_(INVALID_ID)
, fd_(INVALID_ID)
, committed_(false)
, image_(EGL_NO_IMAGE_KHR)
, texture_(INVALID_ID)
, fbo_(INVALID_ID)
{

}

GraphicsBuffer::~GraphicsBuffer()
{

}

bool GraphicsBuffer::Create()
{
    PlaneControllerClient::GraphicsFbClientBuffer fb{};
    if (client_->createGraphicsFb(DISPLAY_WIDTH, DISPLAY_HEIGHT, &fb) != 0)
    {
        ERROR("GraphicsBuffer Create: createGraphicsFb failed");
        return false;
    }

    bufferId_ = fb.graphicsFbId;
    width_ = fb.width;
    height_ = fb.height;
    stride_ = fb.stride;
    offset_ = fb.offset;
    format_ = fb.format;
    fd_ = fb.fd;

    DEBUG("GraphicsBuffer Create: id=%u w=%u h=%u str=%u off=%u fmt=%u fd=%d",
           bufferId_, width_, height_, stride_, offset_, format_, fd_);

    return true;
}

bool GraphicsBuffer::Commit()
{
   DEBUG("GraphicsBuffer Commit %u", bufferId_);

   bool accepted = false;
   if (client_->commitGraphicsFb(bufferId_, &accepted) != 0)
   {
      ERROR("GraphicsBuffer Commit %u failed on commitGraphicsFb", bufferId_);
      return 1;
   }

   committed_ = accepted;
   DEBUG("GraphicsBuffer Commit %u %s", bufferId_, accepted ? "accepted" : "not accepted");

   return accepted;
}

bool GraphicsBuffer::WaitOnRelease()
{
    DEBUG("GraphicsBuffer WaitOnRelease: begin wait %u", bufferId_);

    int32_t releasedId = -1;
    if (client_->waitGraphicsFbReleased(&releasedId) != 0)
    {
        ERROR("GraphicsBuffer WaitOnRelease: waitGraphicsFbReleased failed");
        return false;
   }

   // AIDL listener reports -1 when no previous framebuffer exists, e.g. first commit.
   // Continue waiting until an actual reusable framebuffer id is released.
   while (releasedId < 0)
   {
      if (client_->waitGraphicsFbReleased(&releasedId) != 0)
      {
         ERROR("GraphicsBuffer WaitOnRelease: waitGraphicsFbReleased failed");
         return false;
      }
   }

   if (bufferId_ != static_cast<uint32_t>(releasedId))
   {
        ERROR("GraphicsBuffer WaitOnRelease: wrong bufferId_ %u, expected %d",
               bufferId_, releasedId);
   }
   else
   {
       DEBUG("GraphicsBuffer WaitOnRelease done %u", bufferId_);
   }
   return true;
}

bool GraphicsBuffer::Destroy()
{
    if (!client_->destroyBuffer(bufferId_))
    {
        ERROR("GraphicsBuffer Destroy %d failed", bufferId_);
        return false;
    }
    else
    {
        DEBUG("GraphicsBuffer Destroy %d success", bufferId_);
        return true;
    }
}
#include <drm/drm_fourcc.h>
bool GraphicsBuffer::Load()
{
   INFO("GraphicsBuffer Load w=%u h=%u fmt=%X fd=%d off=%u str=%u", width_, height_, format_, fd_, offset_, stride_);
   /*
   WGLINFOS 16412141191 00063 GraphicsBuffer Load w=1920 h=1080 fmt=34325241 fd=27 off=0 str=7680
   WGLINFOS 16412141208 00017 DRM_FORMAT_XRGB8888=34325258
   WGLINFOS 16412141220 00012 DRM_FORMAT_XBGR8888=34324258
   WGLINFOS 16412141232 00012 DRM_FORMAT_RGBX8888=34325852
   WGLINFOS 16412141244 00012 DRM_FORMAT_BGRX8888=34325842
   WGLINFOS 16412141256 00012 DRM_FORMAT_ARGB8888=34325241 > what plane controller is picking 
   WGLINFOS 16412141267 00011 DRM_FORMAT_ABGR8888=34324241 > what wayland buffers come in as when dma buffers
   WGLINFOS 16412141279 00012 DRM_FORMAT_RGBA8888=34324152
   WGLINFOS 16412141290 00011 DRM_FORMAT_BGRA8888=34324142
   Logger::Write(Logger::Info, "DRM_FORMAT_XRGB8888=%X\n", DRM_FORMAT_XRGB8888);
   Logger::Write(Logger::Info, "DRM_FORMAT_XBGR8888=%X\n", DRM_FORMAT_XBGR8888);
   Logger::Write(Logger::Info, "DRM_FORMAT_RGBX8888=%X\n", DRM_FORMAT_RGBX8888);
   Logger::Write(Logger::Info, "DRM_FORMAT_BGRX8888=%X\n", DRM_FORMAT_BGRX8888);
   Logger::Write(Logger::Info, "DRM_FORMAT_ARGB8888=%X\n", DRM_FORMAT_ARGB8888);
   Logger::Write(Logger::Info, "DRM_FORMAT_ABGR8888=%X\n", DRM_FORMAT_ABGR8888);
   Logger::Write(Logger::Info, "DRM_FORMAT_RGBA8888=%X\n", DRM_FORMAT_RGBA8888);
   Logger::Write(Logger::Info, "DRM_FORMAT_BGRA8888=%X\n", DRM_FORMAT_BGRA8888);
   */

   EGLint attrs[] = {
#ifdef PLATFORM_DRM
      EGL_WIDTH,                       (EGLint)width_,
      EGL_HEIGHT,                      (EGLint)height_,
      EGL_GL_COLORSPACE_KHR,           (EGLint)EGL_GL_COLORSPACE_DEFAULT_EXT,
      EGL_LINUX_DRM_FOURCC_EXT,        (EGLint)format_,
      EGL_DMA_BUF_PLANE0_FD_EXT,       (EGLint)fd_,
      EGL_DMA_BUF_PLANE0_OFFSET_EXT,   (EGLint)offset_,
      EGL_DMA_BUF_PLANE0_PITCH_EXT,    (EGLint)stride_,
#endif
      EGL_NONE
   };

   DEBUG("GraphicsBuffer Load calling eglCreateImageKHR");

   image_ = eglFuncs_->eglCreateImageKHR(eglDisplay_, EGL_NO_CONTEXT, 
#ifdef PLATFORM_BRCM    
        EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)offset_
#else
        EGL_LINUX_DMA_BUF_EXT, NULL, 
#endif
        attrs);

   if (image_ == EGL_NO_IMAGE_KHR)
   {
      ERROR("GraphicsBuffer Load: eglCreateImageKHR error 0x%X", eglGetError());
      return false;
   }

   INFO("GraphicsBuffer Load: eglCreateImageKHR image=%p", image_);

   GLuint tex = 0;
   glGenTextures(1, &tex);
   glBindTexture(GL_TEXTURE_2D, tex);
   eglFuncs_->glEGLImageTargetTexture2DOES(GL_TEXTURE_2D/*GL_TEXTURE_EXTERNAL_OES*/, (GLeglImageOES)image_);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   GLenum glerr = glGetError();
   if (glerr != GL_NO_ERROR)
   {
      ERROR("GraphicsBuffer Load: glEGLImageTargetTexture2DOES error 0x%X", glerr);
      glDeleteTextures(1, &tex);
      eglFuncs_->eglDestroyImageKHR(eglDisplay_, image_);
      image_ = NULL;
      return false;
   }

   texture_ = tex;

   INFO("GraphicsBuffer Load: glEGLImageTargetTexture2DOES success texture=%u", texture_);

   GLuint fbo = 0;
   glGenFramebuffers(1, &fbo);
   INFO("GraphicsBuffer Load calling real glBindFramebuffer fbo=%u", fbo);
   eglFuncs_->glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);
   GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
   if (status != GL_FRAMEBUFFER_COMPLETE)
   {
      ERROR("GraphicsBuffer Load: glFramebufferTexture2D error: 0x%X", status);
      glDeleteFramebuffers(1, &fbo);
      glDeleteTextures(1, &tex);
      eglFuncs_->eglDestroyImageKHR(eglDisplay_, image_);
      texture_ = INVALID_ID;
      image_ = NULL;
      return false;
   }
   INFO("GraphicsBuffer Load calling real glBindFramebuffer fbo=0 (unbind)");
   eglFuncs_->glBindFramebuffer(GL_FRAMEBUFFER, 0);

   fbo_ = fbo;

   INFO("GraphicsBuffer Load success: fbo=%u", fbo_);

   return true;
}