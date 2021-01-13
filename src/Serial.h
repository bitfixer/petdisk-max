#ifndef __serial_h__
#define __serial_h__

class Serial {
public:
    Serial() {}

    ~Serial() {}

    void init(unsigned int ubrr, bool double_speed = false);
    void transmitByte(unsigned char data);
    unsigned char receiveByte();
    void transmitString(unsigned char* string);
    void enable_interrupt();
    void disable_interrupt();
};

class Serial1 {
public:
    Serial1() {}

    ~Serial1() {}

    void init(unsigned int ubrr);
    void transmitByte(char data);
    void transmitString(char* string);
    void transmitStringF(const char* string);
};

#endif