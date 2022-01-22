#ifndef __hardware_h__
#define __hardware_h__

#ifdef ISAVR
#include "hardware_avr.h"
#else
#include "hardware_esp32.h"
#endif

#include <stdint.h>
#include <stddef.h>

#define PACKED __attribute__ ((packed))

uint8_t spi_cs();
void prog_init();
void reset_esp();
void init_led();
void set_led(bool value);
void hDelayMs(int ms);
uint8_t bf_pgm_read_byte(uint8_t* src);

void bf_eeprom_write_block(const void* block, void* eeprom, size_t n);
void bf_eeprom_read_block(void* block, const void* eeprom, size_t n);
uint8_t bf_eeprom_read_byte(const uint8_t* addr);

void spi_init();
uint8_t spi_transmit(uint8_t data);
void spi_cs_select();
void spi_cs_unselect();

void serial0_init(uint32_t baudRate);
void serial0_transmitByte(unsigned char data);
unsigned char serial0_receiveByte();
void serial0_enable_interrupt();
void serial0_disable_interrupt();

void serial1_init(uint32_t baudRate);
void serial1_transmitByte(unsigned char data);
unsigned char serial1_receiveByte();
void serial1_enable_interrupt();
void serial1_disable_interrupt();

#endif