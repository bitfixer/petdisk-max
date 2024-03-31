#include "Serial.h"
#include "hardware.h"
#include <string.h>

namespace bitfixer {

// Serial Port 0
void Serial0::init(uint32_t baudRate)
{
    serial0_init(baudRate);
}

void Serial0::transmitByte(unsigned char data)
{
    serial0_transmitByte(data);
}
    
unsigned char Serial0::receiveByte()
{
    return serial0_receiveByte();
}

void Serial0::enable_interrupt()
{
    serial0_enable_interrupt();
}
    
void Serial0::disable_interrupt()
{
    serial0_disable_interrupt();
}


// Serial Port 1
void Serial1::init(uint32_t baudRate)
{
    serial1_init(baudRate);
}

void Serial1::transmitByte(unsigned char data)
{
    serial1_transmitByte(data);
}

unsigned char Serial1::receiveByte()
{
    return serial1_receiveByte();
}

void Serial1::enable_interrupt()
{
    serial1_enable_interrupt();
}
    
void Serial1::disable_interrupt()
{
    serial1_disable_interrupt();
}


// shared
void Serial::transmitString(const char* str)
{
    while (*str != 0) 
    {
        transmitByte(*str++);
    }
}

void Serial::transmitStringF(const char* string)
{
    uint8_t* str = (uint8_t*)string;
    while (bf_pgm_read_byte(&(*str)))
    {
        transmitByte(bf_pgm_read_byte(&(*str++)));
    }
}

}