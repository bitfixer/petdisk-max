#include <avr/interrupt.h>

extern uint8_t* _buffer;
extern uint16_t _bufferSize;

extern void setup();
extern void loop();

int main(void)
{
    setup();
    while(1) {
        loop();
    }
}

// interrupt handler - should read bytes into a buffer.
// this can be read outside the handler to check for end tokens
// would need to be consumed fast enough to prevent buffer overrun
// length can be an atomic (8-bit) value
// you are guaranteed to be able to read this amount of data
// if you allocate enough space to handle any potential reads, you 
// don't even need a ring buffer, just a regular buffer

// turn on serial interrupts to start reading
// turn off when done
ISR(USART0_RX_vect)
{
    // insert character into buffer
    // loop to beginning if needed
    uint8_t a = UDR0;
    _buffer[_bufferSize] = a;
    _bufferSize++;
}