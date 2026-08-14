#include <cstdio>
#include <vector>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <unistd.h>
#include "debugging.h"
#include "logger.h"
#include "time.h"

static const char* FILEPATH_DUMPFB = "/opt/westeros_gl_dumpfb";

static bool createResolveFramebuffer(
    int width, 
    int height, 
    GLuint *presolveFbo, 
    GLuint *presolveTexture,
    EGLFuncs* eglFuncs)
{
    GLuint resolveFbo = 0;
    GLuint resolveTexture = 0;

    glGenFramebuffers(1, &resolveFbo);
    eglFuncs->glBindFramebuffer(GL_FRAMEBUFFER, resolveFbo);

    glGenTextures(1, &resolveTexture);
    glBindTexture(GL_TEXTURE_2D, resolveTexture);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        resolveTexture,
        0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        ERROR("createResolveFramebuffer failed status=%X", status);
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    eglFuncs->glBindFramebuffer(GL_FRAMEBUFFER, 0);

    *presolveFbo = resolveFbo;
    *presolveTexture = resolveTexture;

    INFO("createResolveFramebuffer success fbo=%u", resolveFbo);

    return true;
}

static bool resolveFramebuffer(
    GLuint multisampleFbo,
    GLuint resolveFbo,
    int width,
    int height,
    EGLFuncs* eglFuncs)
{
    eglFuncs->glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampleFbo);
    eglFuncs->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFbo);

    glReadBuffer(GL_COLOR_ATTACHMENT0);
    //glDrawBuffer(GL_COLOR_ATTACHMENT0);

    glBlitFramebuffer(
        0, 0, width, height,
        0, 0, width, height,
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST);

    GLenum error = glGetError();

    eglFuncs->glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    eglFuncs->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    if (error != GL_NO_ERROR)
    {
        ERROR("resolveFramebuffer failed: glError: %X", error);
        return false;
    }

    return true;
}

bool dumpFramebufferToPPM(
    GLuint framebuffer,
    int width,
    int height,
    const char* filename,
    EGLFuncs* eglFuncs)
{
    //assumes BBP=4 (e.g. RGBA etc,..)
    INFO("dumpFramebufferToPPM fbo=%u %dx%d %s", framebuffer, width, height, filename);

    const size_t rowSize4 = width * 4;
    std::vector<unsigned char> pixelSource(rowSize4 * height);

    GLuint resolveFbo = 0;
    GLuint resolveTexture = 0;

    if (!createResolveFramebuffer(width, height, &resolveFbo, &resolveTexture, eglFuncs))
    {
        ERROR("dumpFramebufferToPPM failed: createResolveFramebuffer");
        return false;
    }

    if (resolveFbo == framebuffer)
    {
        INFO("dumpFramebufferToPPM fbo=%u is the resolve buffer so we skip this one", framebuffer);
        glDeleteTextures(1, &resolveTexture);
        glDeleteFramebuffers(1, &resolveFbo);
        return false;
    }

    if (!resolveFramebuffer(framebuffer, resolveFbo, width, height, eglFuncs))
    {
        ERROR("dumpFramebufferToPPM failed: resolveFramebuffer");
        glDeleteTextures(1, &resolveTexture);
        glDeleteFramebuffers(1, &resolveFbo);      
        return false;
    }

    eglFuncs->glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveFbo);

    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glReadPixels(
        0, 0,
        width, height,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixelSource.data());

    GLenum error = glGetError();

    eglFuncs->glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    glDeleteTextures(1, &resolveTexture);
    glDeleteFramebuffers(1, &resolveFbo);

    if (error != GL_NO_ERROR)
    {
        ERROR("dumpFramebufferToPPM failed: glError: %X", error);
        return false;
    }

    FILE* file = std::fopen(filename, "wb");

    if (!file)
    {
        ERROR("dumpFramebufferToPPM failed to open %s for writing", filename);
        return false;
    }

    const size_t rowSize3 = width * 3;
    std::vector<unsigned char> pixelDest(rowSize3 * height);

    for (int y = height - 1; y >= 0; --y)
    {
        for (int x = 0; x < width;  ++x)
        {
            unsigned char* src = pixelSource.data() + (y * rowSize4) + (x * 4);
            unsigned char* dst = pixelDest.data()   + (y * rowSize3) + (x * 3);
            memcpy(dst, src, 3);
        }
    }

    std::fprintf(file, "P6\n%d %d\n255\n", width, height);//ppm header
    std::fwrite(pixelDest.data(), 1, pixelDest.size(), file);
    std::fclose(file);

    INFO("dumpFramebufferToPPM wrote %s", filename);
    return true;
}

void debugDumpFramebuffersCheck(EGLFuncs* eglFuncs)
{
    static long long lastCheckTime = GetTimeUS();
    long long now = GetTimeUS();
    if (now - lastCheckTime > 1000000)
    {
        lastCheckTime = now;

        if (access(FILEPATH_DUMPFB, F_OK) == 0)
        {
            unlink(FILEPATH_DUMPFB);

            dumpFramebufferToPPM(1, 1920, 1080, "/tmp/fb1.ppm", eglFuncs);
            dumpFramebufferToPPM(2, 1920, 1080, "/tmp/fb2.ppm", eglFuncs);
            dumpFramebufferToPPM(3, 1920, 1080, "/tmp/fb3.ppm", eglFuncs);//the embedders fbo (if any)
        }
    }
}