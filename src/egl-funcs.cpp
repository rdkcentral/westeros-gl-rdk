#include <dlfcn.h>
#include "egl-funcs.h"
#include "logger.h"

EGLFuncs::EGLFuncs()
: eglGetDisplay(nullptr)
, eglSwapBuffers(nullptr)
, eglGetPlatformDisplayEXT(nullptr)
, eglCreateImageKHR(nullptr)
, eglDestroyImageKHR(nullptr)
, glEGLImageTargetTexture2DOES(nullptr)
, inited_(false)
{

}
    
bool EGLFuncs::Init()
{
   if (inited_)
      return false;
   inited_ = true;

   //dlsym sections

   void* eglHandle = dlopen("libEGL.so", RTLD_NOW | RTLD_NOLOAD);

   if (!eglHandle)
   {
      ERROR("dlopen: %s", dlerror());
   }

   if (!eglHandle)
   {
      // It may not yet be loaded, or its recorded SONAME may differ.
      eglHandle = dlopen("libEGL.so", RTLD_NOW | RTLD_LOCAL);

      if (!eglHandle)
      {
         ERROR("dlopen: %s", dlerror());
         return false;
      }
   }

   eglGetDisplay = (PREALEGLGETDISPLAY)dlsym(eglHandle, "eglGetDisplay");
   if (!eglGetDisplay)
   {
      ERROR("EGLFuncs Init eglGetDisplay sym not found: %s", dlerror());
      return false;
   }

   eglSwapBuffers = (PREALEGLSWAPBUFFERS)dlsym(eglHandle, "eglSwapBuffers");
   if (!eglSwapBuffers)
   {
      ERROR("EGLFuncs Init eglSwapBuffers sym not found: %s", dlerror());
      return false;
   }

   eglCreateWindowSurface = (PREALEGLCREATEWINDOWSURFACE)dlsym(eglHandle, "eglCreateWindowSurface");
   if (!eglCreateWindowSurface)
   {
      ERROR("EGLFuncs Init eglCreateWindowSurface sym not found: %s", dlerror());
      return false;
   }

   eglCreatePlatformWindowSurface = (PREALEGLCREATEPLATFORMWINDOWSURFACE)dlsym(eglHandle, "eglCreatePlatformWindowSurface");
   if (!eglCreatePlatformWindowSurface)
   {
      ERROR("EGLFuncs Init eglCreatePlatformWindowSurface sym not found: %s", dlerror());
      return false;
   }

   eglDestroySurface = (PREALEGLDESTROYSURFACE)dlsym(eglHandle, "eglDestroySurface");
   if (!eglDestroySurface)
   {
      ERROR("EGLFuncs Init eglDestroySurface sym not found: %s", dlerror());
      return false;
   }

   eglQuerySurface = (PREALEGLQUERYSURFACE)dlsym(eglHandle, "eglQuerySurface");
   if (!eglQuerySurface)
   {
      ERROR("EGLFuncs Init eglQuerySurface sym not found: %s", dlerror());
      return false;
   }

   eglMakeCurrent = (PREALEGLMAKECURRENT)dlsym(eglHandle, "eglMakeCurrent");
   if (!eglMakeCurrent)
   {
      ERROR("EGLFuncs Init eglMakeCurrent sym not found: %s", dlerror());
      return false;
   }   

   glBindFramebuffer = (PREALGLBINDFRAMEBUFFER)dlsym(eglHandle, "glBindFramebuffer");
   if (!glBindFramebuffer)
   {
      ERROR("EGLFuncs Init glBindFramebuffer sym not found: %s", dlerror());
      return false;
   }      

   //eglGetProcAddress section

   eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
   if (!eglGetPlatformDisplayEXT)
   {
      ERROR("EGLFuncs Init eglGetPlatformDisplayEXT proc not found, egl err=0x%X", eglGetError());
      return false;
   }

   eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
   if (!eglCreateImageKHR)
   {
      ERROR("EGLFuncs Init eglCreateImageKHR proc not found, egl err=0x%X", eglGetError());
      return false;
   }

   eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
   if (!eglDestroyImageKHR)
   {
      ERROR("EGLFuncs Init eglDestroyImageKHR proc not found, egl err=0x%X", eglGetError());
      return false;
   }

   glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
   if (!glEGLImageTargetTexture2DOES)
   {
      ERROR("EGLFuncs Init glEGLImageTargetTexture2DOES proc not found, egl err=0x%X", eglGetError());
      return false;
   }

   return true;
}