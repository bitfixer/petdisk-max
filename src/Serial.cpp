#include "Serial.h"
#include <avr/io.h>
#include <string.h>
#include <avr/pgmspace.h>

uint32_t bRnd(float f)
{
    uint32_t i = (uint32_t)f;
    float r = f - (float)i;

    if (r >= 0.5f)
    {
        return i+1;
    }
    else
    {
        return i;
    }
}

uint8_t ubrrFromBaudRate(uint32_t baudRate, bool* doubleSpeed)
{
    // try regular speed first
    float fRegUbrr = (float(F_CPU) / float(16*baudRate)) - 1.0f;
    uint32_t regUbrr = bRnd(fRegUbrr);
    // get real baud rate
    uint32_t actualRegBr = (float(F_CPU) / float(16*(regUbrr+1)));
    float regErr = 1.0 - (float)actualRegBr / (float)baudRate;
    if (regErr < 0.0) {
        regErr = -regErr;
    }

    // try double speed
    float fDblUbrr = (float(F_CPU) / float(8*baudRate)) - 1.0f;
    uint32_t dblUbrr = bRnd(fDblUbrr);
    // get real baud rate
    uint32_t actualDblBr = (F_CPU / (8*(dblUbrr+1)));
    float dblErr = 1.0 - (float)actualDblBr / (float)baudRate;
    if (dblErr < 0.0) {
        dblErr = -dblErr;
    }

    if (regErr <= dblErr) {
        *doubleSpeed = false;
        return (uint8_t)regUbrr;
    } else {
        *doubleSpeed = true;
        return (uint8_t)dblUbrr;
    }
}

namespace bitfixer {

// Serial Port 0
void Serial0::init(uint32_t baudRate)
{
    bool double_speed;
    uint8_t ubrr = ubrrFromBaudRate(baudRate, &double_speed);

    UBRR0H = (unsigned char)(ubrr>>8);
    UBRR0L = (unsigned char)ubrr;
    UCSR0A = double_speed ? (1<<U2X0) : 0x00;
    UCSR0B = (1<<RXEN0)|(1<<TXEN0);
    UCSR0C = (1<<USBS0)|(3<<UCSZ00);
}

void Serial0::transmitByte(unsigned char data)
{
    while ( !(UCSR0A & (1<<UDRE0)) ) {}
    UDR0 = data;
}
    
unsigned char Serial0::receiveByte()
{
    unsigned char data;
    while ( !(UCSR0A & (1<<RXC0)) ) {}
    data = UDR0;
    return(data);
}

void Serial0::enable_interrupt()
{
    UCSR0B |= (1 << RXCIE0);
}
    
void Serial0::disable_interrupt()
{
    UCSR0B &= ~(1 << RXCIE0);
}


// Serial Port 1
void Serial1::init(uint32_t baudRate)
{
    bool double_speed;
    uint8_t ubrr = ubrrFromBaudRate(baudRate, &double_speed);

    UBRR1H = (unsigned char)(ubrr>>8);
    UBRR1L = (unsigned char)ubrr;
    UCSR1A = double_speed ? (1<<U2X1) : 0x00;
    UCSR1B = (1<<RXEN1)|(1<<TXEN1);
    UCSR1C = (1<<USBS1)|(3<<UCSZ10);
}

void Serial1::transmitByte(unsigned char data)
{
    while ( !(UCSR1A & (1<<UDRE1)) ) {}
    UDR1 = data;
}

unsigned char Serial1::receiveByte()
{
    unsigned char data;
    while ( !(UCSR1A & (1<<RXC1)) ) {}
    data = UDR1;
    return(data);
}

void Serial1::enable_interrupt()
{
    UCSR1B |= (1 << RXCIE1);
}
    
void Serial1::disable_interrupt()
{
    UCSR1B &= ~(1 << RXCIE1);
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
    while (pgm_read_byte(&(*string)))
    {
        transmitByte(pgm_read_byte(&(*string++)));
    }
}

}