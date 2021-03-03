#ifndef __console_logger_h__
#define __console_logger_h__

#include "Logger.h"

#include <stdarg.h>
#include <stdio.h>

class ConsoleLogger : public Logger {
public:
    ConsoleLogger() {};
    ~ConsoleLogger() {};

    void init();
    void log(const char* str);
    void log(unsigned char* data, int length);
    void logF(const char* str);
};

#endif