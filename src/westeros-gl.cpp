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

 /* This module impelements the top level API declared in westeros-gl.h
  * It delegates all the hard work to WesteroGlCtx which 
  * uses the plane controller hal and manages the graphics buffers.
  * Important point here is that we must call LoadBuffers
  * before the client starts drawing with opengl, so we do it
  * when we get the first eglMakeCurrent call from the client
  */

#include "westeros-gl.h"
#include "westeros-gl-context.h"
#include "logger.h"
#include "debugging.h"

typedef struct _WstGLCtx
{
   int refCnt;
   WesteroGlCtx* p;
   bool loadedBuffers;
} WstGLCtx;

/* We don't require, nor need a native window or EGL Surface when rendering to a our graphics buffers.
   The client using westeros-gl may not know this and may try to create these anyways.
   The following WstGLNativeWindow and WstGLEGLSurface are there just to return 'something' to the client
   so that it won't think there's an error if we return NULL. It assumes the client won't do anything 
   except create/destory these, which is all that most client do. */
typedef struct _WstGLNativeWindow
{
} WstGLNativeWindow;

typedef struct _WstGLEGLSurface
{
} WstGLEGLSurface;

static WstGLCtx *gCtx = NULL;
static const char* WESTEROS_UNINITIALIZED_TEXT = "%s failed: WstGLInit has not been called successfully\n";
static const char* WESTEROS_NOIMPL_TEXT = "%s not implemented\n";

WstGLCtx* WstGLInit()
{
   Logger::Init();

   DEBUG("WstGLInit-Debug");
   INFO("WstGLInit-Info");
   ERROR("WstGLInit-Error");
   
   if (gCtx)
   {
      gCtx->refCnt++;
      INFO("westeros-gl WstGLInit refCnt=%d", gCtx->refCnt);
   }   
   else
   {
      WesteroGlCtx* fullCtx = new WesteroGlCtx();

      if (!fullCtx->Init())
      {
         ERROR("westeros-gl WstGLInit failed to initialize context");
         delete fullCtx;
         return NULL;
      }

      gCtx = (WstGLCtx*)calloc(1, sizeof(WstGLCtx));
      gCtx->refCnt = 1;
      gCtx->loadedBuffers = false;
      gCtx->p = fullCtx;

      INFO("westeros-gl WstGLInit context created");
   }
   
   return gCtx;
}

void WstGLTerm( WstGLCtx *ctx )
{
   INFO("westeros-gl WstGLTerm");
   if (ctx && ctx == gCtx)
   {
      --gCtx->refCnt;

      if (gCtx->refCnt <= 0)
      {
         if (gCtx->p)
            delete gCtx->p;

         free(gCtx);
         gCtx = NULL;
      }
   }
   else
   {
      ERROR("WstGLTerm invalid param");
   }
}

EGLAPI EGLDisplay EGLAPIENTRY eglGetDisplay(EGLNativeDisplayType displayId)
{
   INFO("westeros-gl eglGetDisplay");
   //The client may or may not have previously called WstGlInit.  If not, its OK.
   //WstGLInit will correctly setup the EGLDisplay we will return to the client
   if (!gCtx)
   {
      gCtx = WstGLInit();
      if (!gCtx)
         return EGL_NO_DISPLAY;
   }

   if (gCtx->p->GetDisplay() != EGL_NO_DISPLAY)
   {
      DEBUG("eglGetDisplay success display=%p", gCtx->p->GetDisplay());
   }
   else
   {
      ERROR("eglGetDisplay failed to get EGL display");
   }

   return gCtx->p->GetDisplay();
}

EGLAPI EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType native, EGLint const* attrs)
{
   INFO("westeros-gl eglCreateWindowSurface");
   if (!gCtx)
   {
      Logger::Write(Logger::Error, WESTEROS_UNINITIALIZED_TEXT, __func__);
      return EGL_FALSE;
   }

   //This is the logical time to create all our plane control buffers but it won't work because eglMakeCurrent hasn't happened yet
   //return a fake surface because we don't want one
   struct _WstGLEGLSurface* fakeSurface = (struct _WstGLEGLSurface*)calloc(1, sizeof(struct _WstGLEGLSurface));
   return fakeSurface;
}

EGLAPI EGLSurface eglCreatePlatformWindowSurface(EGLDisplay dpy, EGLConfig config, void* native, EGLint const* attrs)
{
   INFO("westeros-gl eglCreatePlatformWindowSurface");
   if (!gCtx)
   {
      Logger::Write(Logger::Error, WESTEROS_UNINITIALIZED_TEXT, __func__);
      return EGL_FALSE;
   }

   //There is no typical window surface when using our graphics buffers, so we return a fake one.
   //If we return NULL, the client might error out.
   struct _WstGLEGLSurface* fakeSurface = (struct _WstGLEGLSurface*)calloc(1, sizeof(struct _WstGLEGLSurface));
   return fakeSurface;
}

EGLAPI EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface)
{
   INFO("westeros-gl eglDestroySurface");
   if (!gCtx)
   {
      Logger::Write(Logger::Error, WESTEROS_UNINITIALIZED_TEXT, __func__);
      return EGL_FALSE;
   }

   if(surface)
      free(surface);
   
   return EGL_TRUE;
}

EGLAPI EGLBoolean eglQuerySurface(EGLDisplay display, EGLSurface surface, EGLint attribute, EGLint* value)
{
   DEBUG("westeros-gl eglQuerySurface");
   if (!gCtx)
   {
      Logger::Write(Logger::Error, WESTEROS_UNINITIALIZED_TEXT, __func__);
      return EGL_FALSE;
   }

   //TODO -- could we potentially support this on our plane control surface ?
   return EGL_FALSE;
}

EGLAPI EGLBoolean eglMakeCurrent(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context)
{
   EGLBoolean ret;

   DEBUG("westeros-gl eglMakeCurrent");

   if (!gCtx)
   {
      Logger::Write(Logger::Error, WESTEROS_UNINITIALIZED_TEXT, __func__);
      return EGL_FALSE;
   }

   //If they aren't passing in the display returned from eglGetDisplay, we're in trouble
   if (display != gCtx->p->GetDisplay())
   {
      ERROR("eglMakeCurrent invalide display parameter");
      return EGL_FALSE;
   }

   //we have to take over here and send EGL_NO_SURFACE to the real eglMakeCurrent since we are controlling the surface rendering
   // and we don't want EGL, if it does it, creating additional, unnecessary buffers we can't use
   (void)draw;
   (void)read;

   ret = gCtx->p->GetEGLFuncs()->eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);

   //We also call LoadBuffers from eglCreateWindowSurface, but what if the client is smart and doesn't call that.
   //So we call here, when we have a display and a context and it looks like the client has selected their initial context
   if (!gCtx->loadedBuffers && ret == EGL_TRUE && display && context)
   {
      if (gCtx->p->LoadBuffers())
      {
         gCtx->loadedBuffers = true;
      }
      else
      {
         ERROR("eglMakeCurrent LoadBuffers failed");
         return EGL_FALSE;
      }
   }
   return ret;
}

EGLAPI EGLBoolean eglSwapBuffers( EGLDisplay display, EGLSurface surface )
{
   if (!gCtx)
   {
      Logger::Write(Logger::Error, WESTEROS_UNINITIALIZED_TEXT, __func__);
      return EGL_FALSE;
   }

   //If they aren't passing in the display returned from eglGetDisplay, we're in trouble
   if (display != gCtx->p->GetDisplay())
   {
      ERROR("eglSwapBuffers invalide display parameter");
      return EGL_FALSE;
   }

   debugDumpFramebuffersCheck(gCtx->p->GetEGLFuncs());

   if (gCtx->p->SwapBuffers())
      return EGL_TRUE;
   else
      return EGL_FALSE;
}

void glBindFramebuffer(GLenum target, GLuint framebuffer)
{
   DEBUG("westeros-gl glBindFramebuffer called target=%X fbo=%u", target, framebuffer);
   if (!gCtx)
   {
      Logger::Write(Logger::Error, WESTEROS_UNINITIALIZED_TEXT, __func__);
      return;
   }

   //We have to override glBindFramebuffer because the client itself may be using
   //FBOs to do embedded composing.  After composing, a client will unbind their FBO
   //by calling glBindFramebuffer(GL_FRAMEBUFFER, 0).  This tells opengl to bind
   //to the EGL context's surface (e.g. the primary display surface).  In our case,
   //the EGL context doesn't have a surface.  We provide a 'surface' with our 
   //graphics buffers, and in order for the client to be able to draw into them
   //we have to replace fbo 0 with our current swap chain's graphics buffers fbo
   gCtx->p->BindFrameBuffer(target, framebuffer);
}

void* WstGLCreateNativeWindow( WstGLCtx *ctx, int x, int y, int width, int height )
{
   struct _WstGLNativeWindow* fakeNativeWindow = (struct _WstGLNativeWindow*)calloc(1, sizeof(struct _WstGLNativeWindow));
   return fakeNativeWindow;
}

void WstGLDestroyNativeWindow( WstGLCtx *ctx, void *nativeWindow )
{
   INFO("westeros-gl WstGLDestroyNativeWindow");
   if (nativeWindow)
      free(nativeWindow);
   return;
}

//The remaining function may or may not need to be implemented. We shall see.
bool WstGLSetDisplayMode( WstGLCtx *ctx, const char *mode )
{
   Logger::Write(Logger::Info, WESTEROS_NOIMPL_TEXT, __func__);
   return false;
}

bool WstGLGetDisplayInfo( WstGLCtx *ctx, WstGLDisplayInfo *displayInfo )
{
   Logger::Write(Logger::Info, WESTEROS_NOIMPL_TEXT, __func__);
   return false;
}

bool WstGLGetDisplaySafeArea( WstGLCtx *ctx, int *x, int *y, int *w, int *h )
{
   Logger::Write(Logger::Info, WESTEROS_NOIMPL_TEXT, __func__);
   return false;
}

bool WstGLAddDisplaySizeListener( WstGLCtx *ctx, void *userData, WstGLDisplaySizeCallback listener )
{
   Logger::Write(Logger::Info, WESTEROS_NOIMPL_TEXT, __func__);
   return false;
}

bool WstGLRemoveDisplaySizeListener( WstGLCtx *ctx, WstGLDisplaySizeCallback listener )
{
   Logger::Write(Logger::Info, WESTEROS_NOIMPL_TEXT, __func__);
   return false;
}

bool WstGLGetNativePixmap( WstGLCtx *ctx, void *nativeBuffer, void **nativePixmap )
{
   Logger::Write(Logger::Info, WESTEROS_NOIMPL_TEXT, __func__);
   return false;
}

void WstGLGetNativePixmapDimensions( WstGLCtx *ctx, void *nativePixmap, int *width, int *height )
{
   Logger::Write(Logger::Info, WESTEROS_NOIMPL_TEXT, __func__);
   return;
}

void WstGLReleaseNativePixmap( WstGLCtx *ctx, void *nativePixmap )
{
   Logger::Write(Logger::Info, WESTEROS_NOIMPL_TEXT, __func__);
   return;
}

void* WstGLGetEGLNativePixmap( WstGLCtx *ctx, void *nativePixmap )
{
   Logger::Write(Logger::Info, WESTEROS_NOIMPL_TEXT, __func__);
   return NULL;
}
