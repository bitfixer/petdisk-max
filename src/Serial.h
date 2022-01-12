#ifndef __serial_h__
#define __serial_h__

#include <stdint.h>

namespace bitfixer
{

class Serial {
public:
    Serial() {}
    ~Serial() {}

    virtual void init(uint32_t baudRate);
    virtual void transmitByte(unsigned char data) = 0;
    virtual unsigned char receiveByte() = 0;
    virtual void transmitString(const char* string);
    virtual void transmitStringF(const char* string);
    virtual void enable_interrupt() = 0;
    virtual void disable_interrupt() = 0;
};

class Serial0 : public Serial {
public:
    Serial0() {}
    ~Serial0() {}

    void init(uint32_t baudRate);
    void transmitByte(unsigned char data);
    unsigned char receiveByte();
    void enable_interrupt();
    void disable_interrupt();
};

class Serial1 : public Serial {
public:
    Serial1() {}
    ~Serial1() {}

    void init(uint32_t baudRate);
    void transmitByte(unsigned char data);
    unsigned char receiveByte();
    void enable_interrupt();
    void disable_interrupt();
};

}

#endif