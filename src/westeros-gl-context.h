#pragma once 

#include "egl-funcs.h"
#include "plane-controller-client.h"

#define SWAP_CHAIN_COUNT (2)

class GraphicsBuffer;

#ifdef PLATFORM_DRM    
struct gbm_device;
#endif

class WesteroGlCtx
{
public:
   WesteroGlCtx();
   ~WesteroGlCtx();
   bool Init();
   bool LoadBuffers();
   bool SwapBuffers();
   void BindFrameBuffer(GLenum target, GLuint framebuffer);
   EGLDisplay GetDisplay(){ return eglDisplay_; }
   EGLFuncs* GetEGLFuncs() { return &eglFuncs_; }
#ifdef PLATFORM_DRM    
   struct gbm_device* getGBM() { return gbmDevice_; }
#endif   
private:
   EGLFuncs eglFuncs_;
   PlaneControllerClient client_;
   GraphicsBuffer* buffers_[SWAP_CHAIN_COUNT];
   int frontBufferIndex_;
   EGLDisplay eglDisplay_;
   #ifdef PLATFORM_DRM 
   int drmFd_;
   struct gbm_device* gbmDevice_;
   #endif
};