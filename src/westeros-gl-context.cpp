/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "westeros-gl-context.h"
#include "logger.h"
#include "graphics-buffer.h"
#ifdef PLATFORM_DRM
//unfortunately drm/gbm is required just to create an egl display, even when using plane controller
#include <drm/drm_fourcc.h>
#include <gbm.h>
#define DEFAULT_CARD "/dev/dri/card0"
#endif

int DISPLAY_WIDTH = 1920;
int DISPLAY_HEIGHT = 1080;


WesteroGlCtx::WesteroGlCtx()
: frontBufferIndex_(0)
, eglDisplay_(EGL_NO_DISPLAY)
  #ifdef PLATFORM_DRM 
, drmFd_(0)
, gbmDevice_(NULL)
  #endif
{
   for (int i = 0; i < SWAP_CHAIN_COUNT; ++i)
      buffers_[i] = nullptr;
}

WesteroGlCtx::~WesteroGlCtx()
{
   client_.close();

   #ifdef PLATFORM_DRM         
   if (gbmDevice_)
      gbm_device_destroy(gbmDevice_);
   if (drmFd_ > 0)
      close(drmFd_);
   #endif

   for(int i = 0; i < SWAP_CHAIN_COUNT; ++i)
      if (buffers_[i])
         delete buffers_[i];
}

bool WesteroGlCtx::Init()
{
   if (!eglFuncs_.Init())
   {
      ERROR("WesteroGlCtx failed to initialize EGL funcs");
      return NULL;
   }

#ifdef PLATFORM_DRM
   // DRM appears to require all this to get a native EGLDisplay
   const char* card = getenv("WESTEROS_DRM_CARD");
   if (!card)
   {
      card = DEFAULT_CARD;
   }

   drmFd_ = open(card, O_RDWR|O_CLOEXEC);
   if (drmFd_ < 0)
   {
      ERROR("WesteroGlCtx failed to open drm card %s", card);
      return false;
   }

   gbmDevice_ = gbm_create_device(drmFd_);
   if (!gbmDevice_)
   {
      ERROR("WesteroGlCtx gbm_create_device failed");
      return false;
   }

   eglDisplay_ = eglFuncs_.eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, gbmDevice_, NULL);
   if (eglDisplay_ == EGL_NO_DISPLAY)
      ERROR("eglGetPlatformDisplayEXT EGL_PLATFORM_GBM_KHR failed");
   else
      DEBUG("eglGetPlatformDisplayEXT EGL_PLATFORM_GBM_KHR success display=%p", eglDisplay_);

#else
#ifdef PLATFORM_BRCM

   eglDisplay_ = eglFuncs_.eglGetPlatformDisplayEXT(EGL_PLATFORM_NEXUS_BRCM, EGL_DEFAULT_DISPLAY, NULL);
   if (eglDisplay_ == EGL_NO_DISPLAY)
      ERROR("eglGetPlatformDisplayEXT EGL_PLATFORM_NEXUS_BRCM failed");
   else
      DEBUG("eglGetPlatformDisplayEXT EGL_PLATFORM_NEXUS_BRCM success display=%p", eglDisplay_);

#else
   // This fallback probably won't ever work
   eglDisplay_ = eglFuncs_.eglGetDisplay(EGL_DEFAULT_DISPLAY);
   if (eglDisplay_ == EGL_NO_DISPLAY)
      ERROR("eglGetDisplay failed");
   else
      DEBUG("eglGetDisplay success display=%p", eglDisplay_);
   
#endif
#endif
   
   // Connect to the plane control server
   if (client_.connect() < 0)
   {
      ERROR("WesteroGlCtx failed to connect to plane controller");
      return false;
   }

   return true;
}

bool WesteroGlCtx::LoadBuffers()
{
   for (int i = 0; i < SWAP_CHAIN_COUNT; ++i)
   {
      buffers_[i] = new GraphicsBuffer(&client_, &eglFuncs_, eglDisplay_);
      
      // Create involves getting a new buffer from the plane control server
      if (!buffers_[i]->Create())
      {
         ERROR("WesteroGlCtx failed to create plane controller buffer");
         return false;
      }

      // Load the buffer as an opengl framebuffer object so the client can draw into it
      if (!buffers_[i]->Load())
      {
         ERROR("WesteroGlCtx failed to load plane controller buffer");
         return false;
      }
   }

   // Initiate the buffer swapping by starting at index 0
   frontBufferIndex_ = 0;

   // Bind the first buffer so client can draw the first frame into it
   BindFrameBuffer(GL_FRAMEBUFFER, buffers_[frontBufferIndex_]->Fbo());
   
   return true;
}

bool WesteroGlCtx::SwapBuffers()
{
   // This unbinds the current graphics buffer which the client
   // has just finished drawing the current frame graphics into.
   DEBUG("WesteroGlCtx SwapBuffers calling real glBindFramebuffer fbo=0 (unbind)");
   eglFuncs_.glBindFramebuffer(GL_FRAMEBUFFER, 0);

   // Ensure all opengl commands are sent to gpu
   glFlush();
   glFinish();

   // Commit the buffer for the current frame to plane control server
   if (!buffers_[frontBufferIndex_]->Commit())
   {
      ERROR("WesteroGlCtx SwapBuffers Commit failed");
      return false;
   }

   // Wait for plane control server to finish sending it to scanout
   if (!buffers_[frontBufferIndex_]->WaitOnRelease())
   {
      ERROR("WesteroGlCtx SwapBuffers WaitOnRelease failed");
      return false;
   }

   // `Swap` to next buffer
   frontBufferIndex_ = ((frontBufferIndex_+1) % SWAP_CHAIN_COUNT);

   // Bind the next buffer so client can draw the next frame into it
   BindFrameBuffer(GL_FRAMEBUFFER, buffers_[frontBufferIndex_]->Fbo());

   return true;
}

void WesteroGlCtx::BindFrameBuffer(GLenum target, GLuint framebuffer)
{
   //If the client is attempting to restore the framebuffer to 0, then we switch to ours.
   //Otherwise, pass through to the real glBindFramebuffers
   if (target == GL_FRAMEBUFFER && framebuffer == 0)
   {
      GLuint ourFbo = buffers_[frontBufferIndex_]->Fbo();

      DEBUG("WesteroGlCtx BindFrameBuffer calling real glBindFramebuffer fbo=%u (restore)", ourFbo);
      eglFuncs_.glBindFramebuffer(GL_FRAMEBUFFER, ourFbo);
   }
   else
   {
      DEBUG("WesteroGlCtx BindFrameBuffer calling real glBindFramebuffer fbo=%u", framebuffer);
      eglFuncs_.glBindFramebuffer(target, framebuffer);
   }
}