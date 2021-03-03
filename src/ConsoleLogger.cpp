#include "ConsoleLogger.h"

void ConsoleLogger::init() 
{
}

void ConsoleLogger::log(const char* str)
{
    ::printf(str);
}
    
void ConsoleLogger::log(unsigned char* data, int length)
{
    for (int i = 0; i < length; i++)
    {
        ::printf("%c", data[i]);
    }
}

void ConsoleLogger::logF(const char* str) 
{
    
}