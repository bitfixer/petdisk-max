#ifndef __console_logger_h__
#define __console_logger_h__

//#include "Logger.h"

#include <stdarg.h>
#include <stdio.h>

static char _line[64];

class ConsoleLogger {
public:
    ConsoleLogger() {};
    virtual ~ConsoleLogger() {};

    void init();
    void log(const char* str);
    void log(unsigned char* data, int length);
    void logF(const char* str);

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