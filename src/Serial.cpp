#include "Serial.h"
#include <avr/io.h>
#include <string.h>
#include <avr/pgmspace.h>

void Serial::init(unsigned int ubrr, bool double_speed)
{
    UBRR0H = (unsigned char)(ubrr>>8);
    UBRR0L = (unsigned char)ubrr;
    UCSR0A = double_speed ? (1<<U2X0) : 0x00;
    UCSR0B = (1<<RXEN0)|(1<<TXEN0);
    UCSR0C = (1<<USBS0)|(3<<UCSZ00);
}

void Serial::transmitByte(unsigned char data)
{
    while ( !(UCSR0A & (1<<UDRE0)) ) {}
    UDR0 = data;
}

void Serial::transmitString(unsigned char* string)
{
    while (*string) 
    {
        transmitByte(*string++);
    }
}
    
unsigned char Serial::receiveByte()
{
    unsigned char data;
    while ( !(UCSR0A & (1<<RXC0)) ) {}
    data = UDR0;
    return(data);
}
void Serial::enable_interrupt()
{
    UCSR0B |= (1 << RXCIE0);
}
    
void Serial::disable_interrupt()
{
    UCSR0B &= ~(1 << RXCIE0);
}




void Serial1::init(unsigned int ubrr)
{
    UBRR1H = (unsigned char)(ubrr>>8);
    UBRR1L = (unsigned char)ubrr;
    UCSR1A = 0x00;
    UCSR1B = (1<<RXEN1)|(1<<TXEN1);
    UCSR1C = (1<<USBS1)|(3<<UCSZ10);
}

void Serial1::transmitByte(char data)
{
    while ( !(UCSR1A & (1<<UDRE1)) ) {}
    UDR1 = data;
}

void Serial1::transmitString(char* str)
{
    while (*str != 0) 
    {
        transmitByte(*str++);
    }

    /*
    for (int i = 0; i < strlen(str); i++)
    {
        transmitByte(str[i]);
    }
    */
}

void Serial1::transmitStringF(const char* string)
{
    while (pgm_read_byte(&(*string)))
    {
        transmitByte(pgm_read_byte(&(*string++)));
    }
}