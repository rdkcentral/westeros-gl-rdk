#pragma once

class Logger
{
public:
    enum Level
    {
        Debug,
        Info,
        Error,
        Off
    };

    static void Init();
    static void Write(Level level, const char *format, ... );
    static void Flush();
    
    static Level level;
};

#define DEBUG(FORMAT,...)   do { if(Logger::level <= Logger::Debug) Logger::Write( Logger::Debug, FORMAT"\n", ##__VA_ARGS__ ); } while(0)
#define INFO(FORMAT,...)    do { if(Logger::level <= Logger::Info ) Logger::Write( Logger::Info,  FORMAT"\n", ##__VA_ARGS__ ); } while(0)
#define ERROR(FORMAT,...)   do { if(Logger::level <= Logger::Error) Logger::Write( Logger::Error, FORMAT"\n", ##__VA_ARGS__ ); } while(0)
