#include "Serial.h"
#include <avr/io.h>
#include <string.h>
#include <avr/pgmspace.h>

namespace bitfixer {

// Serial Port 0
void Serial0::init(int baudRate)
{
    // temp temp
    uint8_t ubrr = 0;
    bool double_speed = false;

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
void Serial1::init(int baudRate)
{
    // temp temp
    uint8_t ubrr = 0;
    bool double_speed = false;

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