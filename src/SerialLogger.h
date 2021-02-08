#ifndef __serial_logger_h__
#define __serial_logger_h__

#include "Serial.h"

class SerialLogger {
public:
    SerialLogger(Serial* serial)
    : _serial(serial) {

    }

    void init()
    {
    }

    void log(const char* str)
    {
        _serial->transmitString(str);
    }

    void log(unsigned char* data, int length)
    {
        
        for (int l = 0; l < length; l++)
        {
            _serial->transmitByte(data[l]);
        }
    }

private:
    Serial* _serial;
};

#endif