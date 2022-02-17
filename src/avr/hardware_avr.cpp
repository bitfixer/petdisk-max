#include "../hardware.h"

#include <stdio.h>
#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>

#define ESP_CONTROL     DDRD
#define ESP_PORT        PORTD
#define ESP_CH_PD       PD4
#define ESP_RST         PD5
#define ESP_GPIO0       PD7
#define ESP_GPIO2       PD6

#define LED_CONTROL     DDRB
#define LED_PORT        PORTB
#define LED_PIN1        PB0
#define LED_PIN2        PB1

#define SPI_CONTROL     DDRB
#define SPI_PORT        PORTB
#define SPI_CS          PB4
#define SPI_CS2         PB3
#define SPI_MOSI        PB5
#define SPI_SCK         PB7

#define SPI_CS_MASK     1<<SPI_CS
#define NOT_SPI_CS_MASK ~(SPI_CS_MASK)

uint8_t spi_cs()
{
    return SPI_CS;
}

void prog_init()
{
     // set reset for esp
    ESP_PORT = (1 << ESP_CH_PD) | (1 << ESP_RST);
    ESP_CONTROL = (1 << ESP_CH_PD) | (1 << ESP_RST);
    sei();
}

void reset_esp()
{
    ESP_PORT &= ~(1 << ESP_RST);
    ESP_PORT &= ~(1 << ESP_CH_PD);

    for (int i = 0; i < 20; i++)
    {
        _delay_loop_2(65535);
    }

    ESP_PORT |= 1 << ESP_RST;
    ESP_PORT |= 1 << ESP_CH_PD;
    hDelayMs(500);
}

void init_led()
{
    LED_CONTROL |= 1 << LED_PIN1;
    LED_CONTROL |= 1 << LED_PIN2;
}

void set_led(bool value)
{
    if (value == true)
    {
        LED_PORT |= 1 << LED_PIN1;
        LED_PORT |= 1 << LED_PIN2;
    }
    else
    {
        LED_PORT &= ~(1 << LED_PIN1);
        LED_PORT &= ~(1 << LED_PIN2);
    }
}

void hDelayMs(int ms)
{
    for (int i = 0; i < ms; i++)
    {
        _delay_ms(1);
    }
}

uint8_t bf_pgm_read_byte(uint8_t* src)
{
  return pgm_read_byte(src);
}

void bf_eeprom_write_block(const void* block, void* eeprom, size_t n)
{
    eeprom_write_block(block, eeprom, n);
}

void bf_eeprom_read_block(void* block, const void* eeprom, size_t n)
{
    eeprom_read_block(block, eeprom, n);
}

uint8_t bf_eeprom_read_byte(const uint8_t* addr)
{
    return eeprom_read_byte(addr);
}

// SPI

void spi_init()
{
    SPI_PORT = 0x00;
    SPI_PORT |= 1 << SPI_CS;
    SPI_PORT |= 1 << SPI_CS2;
    SPI_CONTROL |= 1 << SPI_CS;
    SPI_CONTROL |= 1 << SPI_CS2;
    SPI_CONTROL |= 1 << SPI_MOSI;
    SPI_CONTROL |= 1 << SPI_SCK; 

    SPCR = 0x52;
    SPSR = 0x00;
}

uint8_t spi_transmit(uint8_t data)
{
    SPDR = data;
    while ( !(SPSR & (1<<SPIF)) ) {}

    data = SPDR;
    return data;
}

void spi_cs_select()
{
    SPI_PORT &= NOT_SPI_CS_MASK;
}

void spi_cs_unselect()
{
    SPI_PORT |= SPI_CS_MASK;
}

// serial

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

void serial0_init(uint32_t baudRate)
{
    bool double_speed;
    uint8_t ubrr = ubrrFromBaudRate(baudRate, &double_speed);

    UBRR0H = (unsigned char)(ubrr>>8);
    UBRR0L = (unsigned char)ubrr;
    UCSR0A = double_speed ? (1<<U2X0) : 0x00;
    UCSR0B = (1<<RXEN0)|(1<<TXEN0);
    UCSR0C = (1<<USBS0)|(3<<UCSZ00);
}

void serial0_transmitByte(unsigned char data)
{
    while ( !(UCSR0A & (1<<UDRE0)) ) {}
    UDR0 = data;
}

unsigned char serial0_receiveByte()
{
    unsigned char data;
    while ( !(UCSR0A & (1<<RXC0)) ) {}
    data = UDR0;
    return(data);
}

void serial0_enable_interrupt()
{
    UCSR0B |= (1 << RXCIE0);
}

void serial0_disable_interrupt()
{
    UCSR0B &= ~(1 << RXCIE0);
}

void serial1_init(uint32_t baudRate)
{
    bool double_speed;
    uint8_t ubrr = ubrrFromBaudRate(baudRate, &double_speed);

    UBRR1H = (unsigned char)(ubrr>>8);
    UBRR1L = (unsigned char)ubrr;
    UCSR1A = double_speed ? (1<<U2X1) : 0x00;
    UCSR1B = (1<<RXEN1)|(1<<TXEN1);
    UCSR1C = (1<<USBS1)|(3<<UCSZ10);
}

void serial1_transmitByte(unsigned char data)
{
    while ( !(UCSR1A & (1<<UDRE1)) ) {}
    UDR1 = data;
}

unsigned char serial1_receiveByte()
{
    unsigned char data;
    while ( !(UCSR1A & (1<<RXC1)) ) {}
    data = UDR1;
    return(data);
}

void serial1_enable_interrupt()
{
    UCSR1B |= (1 << RXCIE1);
}

void serial1_disable_interrupt()
{
    UCSR1B &= ~(1 << RXCIE1);
}

void firmware_detected_action(bitfixer::FAT32* fs, Logger* logger)
{
    // firmware file still exists from previous update
    // delete the file

    fs->deleteFile();
}