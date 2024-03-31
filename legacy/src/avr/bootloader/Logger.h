#ifndef __logger_h__
#define __logger_h__

#include <stdarg.h>
#include <stdio.h>

#define LOG_BUFFER_SIZE 64
static char _line[LOG_BUFFER_SIZE];

class Logger {
public:
    Logger() {}
    ~Logger() {}

    virtual void init() {}
    virtual void log(const char* str) {}
    virtual void log(unsigned char* data, int length) {}
    virtual void logF(const char* str) {}
    virtual void printf(const char* format, ...)
    {
        va_list vl;
        va_start(vl, format);
        vsprintf(_line, format, vl);
        log(_line);
    }
    virtual void flush() {}
};

#endif