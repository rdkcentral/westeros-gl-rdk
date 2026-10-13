#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <cstdlib>
#include <string>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "logger.h"
#include "time.h"

#define LOG_FILE_PATH_DEFAULT "/opt/logs/westeros_gl_%s_%d.log"

Logger::Level Logger::level = Logger::Error;
int Logger::debugLimiter = 1000;

static FILE* logFile = NULL;
static long long lastLogTime = 0;

static int readProcessComm(pid_t pid, char *buffer, size_t size)
{
    char path[64];

    buffer[0] = '\0';

    int n = snprintf(path, sizeof path, "/proc/%ld/comm", (long)pid);
    if (n < 0 || (size_t)n >= sizeof path || size == 0) {
        errno = EINVAL;
        return -1;
    }

    int fd = open(path, O_RDONLY);
    if (fd == -1)
        return -1;

    ssize_t bytes = read(fd, buffer, size - 1);
    int saved_errno = errno;
    close(fd);
    errno = saved_errno;

    if (bytes == -1)
        return -1;

    buffer[bytes] = '\0';

    /* /proc/<pid>/comm ends with a newline. */
    buffer[strcspn(buffer, "\n")] = '\0';
    return 0;
}

void Logger::Init()
{
    if (logFile)
        return;
    
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
    level = Debug;//debugLimiter will only log the first 1000 messages

    char defaultPath[256];

    const char* logPath = std::getenv("WESTEROS_GL_LOG_PATH");

    if (!logPath)
    {
        int pid = getpid();
        char comm[64];
        readProcessComm(pid, comm, sizeof comm);

        snprintf(defaultPath, sizeof defaultPath, LOG_FILE_PATH_DEFAULT, comm, pid);
        logPath = defaultPath;
    }

    logFile = std::fopen(logPath, "w");

    lastLogTime = GetTimeUS();

    if (!logFile)
    {
        logFile = stdout;

        Write(Error, "Failed to open log file %s", logPath);
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
