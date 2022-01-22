#include "hardware.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <SPI.h>

#define LED_PIN     2
#define CS_PIN      4

uint8_t spi_cs()
{
    return CS_PIN;
}

void prog_init()
{
    // nothing for esp32
    set_atn_input();
    EEPROM.begin(512);    
}
 
void reset_esp()
{
    // nothing for esp32
}

void init_led()
{
    // nothing for esp32
    pinMode(LED_PIN, OUTPUT);
}

void set_led(bool value)
{
    if (value == true)
    {
        digitalWrite(2, HIGH);
    }
    else
    {
        digitalWrite(2, LOW);
    }
}

void hDelayMs(int ms)
{
    delay(ms);
}

uint8_t bf_pgm_read_byte(uint8_t* src)
{
    return *src;
}

void bf_eeprom_write_block(const void* block, void* eeprom, size_t n)
{
    EEPROM.writeBytes((int)eeprom, block, n);
    EEPROM.commit();
}

void bf_eeprom_read_block(void* block, const void* eeprom, size_t n)
{
    EEPROM.readBytes((int)eeprom, (void*)block, n);
}

uint8_t bf_eeprom_read_byte(const uint8_t* addr)
{
    return EEPROM.readByte((int)addr);
}

// SPI

void spi_init()
{
    pinMode(CS_PIN, OUTPUT);
    spi_cs_unselect();
    SPI.begin();
}

uint8_t spi_transmit(uint8_t data)
{
    return SPI.transfer(data);
}

void spi_cs_select()
{
    digitalWrite(CS_PIN, LOW);
}

void spi_cs_unselect()
{
    digitalWrite(CS_PIN, 1);
}

// serial

void serial0_init(uint32_t baudRate)
{
    // N/A
}

void serial0_transmitByte(unsigned char data)
{
   // N/A
}

unsigned char serial0_receiveByte()
{
    return 0;
}

void serial0_enable_interrupt()
{
    // N/A
}

void serial0_disable_interrupt()
{
    // N/A
}

void serial1_init(uint32_t baudRate)
{
    ::Serial.begin(baudRate);
}

void serial1_transmitByte(unsigned char data)
{
    ::Serial.write(data);
}

unsigned char serial1_receiveByte()
{
    int ret = ::Serial.read();
    while (ret == -1)
    {
        ret = ::Serial.read();
    }

    return (unsigned char)ret;
}

void serial1_enable_interrupt()
{
    // N/A
}

void serial1_disable_interrupt()
{
    // N/A
}