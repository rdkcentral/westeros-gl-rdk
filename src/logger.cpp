#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <cstdlib>
#include <string>
#include <strings.h>
#include <unistd.h>
#include "logger.h"
#include "time.h"

#define LOG_FILE_PATH_DEFAULT "/opt/logs/westeros-gl.log"

Logger::Level Logger::level = Logger::Error;

static FILE* logFile = NULL;
static long long lastLogTime = 0;

void Logger::Init()
{
    const char* sLevel = std::getenv("WESTEROS_GL_LOG_LEVEL");

    if(sLevel)
    {
        if(strcasecmp(sLevel, "debug") == 0)
            level = Debug;
        else if(strcasecmp(sLevel, "info") == 0)
            level = Info;
        else if(strcasecmp(sLevel, "error") == 0)
            level = Error;
        else if(strcasecmp(sLevel, "off") == 0)
            level = Off;
        //ignore invalids
    }

    const char* logPath = std::getenv("WESTEROS_GL_LOG_PATH");
    if (!logPath)
    {
        logPath = LOG_FILE_PATH_DEFAULT;
    }

    std::string finalPath = std::string(logPath) + "." + std::to_string(getpid());

    logFile = std::fopen(finalPath.c_str(), "w");

    lastLogTime = GetTimeUS();

    if (!logFile)
    {
        logFile = stdout;

        Write(Error, "Failed to open log file %s", finalPath.c_str());
    }
}

void Logger::Write(Level level, const char *format, ... )
{
    static const char* LevelString[] = {
            "Debug",
            "Info",
            "Error",
            ""
    };
    
    if (Logger::level > level)
        return;

    long long now = GetTimeUS();
    int elapsed = (int)(now - lastLogTime);
    lastLogTime = now;

    fprintf(logFile, "WGL%s %09lld %05d ", LevelString[level], now, elapsed);
    va_list vl;
    va_start(vl, format);
    vfprintf(logFile, format, vl);
    va_end(vl);

    Flush();//TODO - improve
}

void Logger::Flush()
{
    fflush(logFile);
}
