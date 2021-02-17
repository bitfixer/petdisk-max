#ifndef __serial_logger_h__
#define __serial_logger_h__

#include "Serial.h"
#define LOG_BUFFER_SIZE 64

class Logger {
public:
    Logger() {}
    ~Logger() {}

    virtual void init() = 0;
    virtual void log(const char* str) = 0;
    virtual void log(unsigned char* data, int length) = 0;
    virtual void logF(const char* str) = 0;
    virtual void flush() {}

};

class SerialLogger : public Logger {
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

    void logF(const char* str)
    {
        _serial->transmitStringF(str);
    }

private:
    Serial* _serial;
};

class BufferedSerialLogger : public Logger {
public:
    BufferedSerialLogger(Serial* serial)
    : _serial(serial)
    , _bufferPos(0) {

    }

    void init()
    {
    }

    void log(const char* str)
    {

    }

    void log(unsigned char* data, int length)
    {

    }

    void logF(const char* str)
    {

    }

    void flush()
    {

    }

private:
    Serial* _serial;
    char _logBuffer[LOG_BUFFER_SIZE];
    unsigned int _bufferPos;
};

#endif