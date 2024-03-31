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
void init_led();
void set_led(bool value);
void hDelayMs(int ms);

int getMs();

uint8_t bf_pgm_read_byte(uint8_t* src);

void bf_eeprom_write_block(const void* block, void* eeprom, size_t n);
void bf_eeprom_read_block(void* block, const void* eeprom, size_t n);
uint8_t bf_eeprom_read_byte(const uint8_t* addr);

void spi_init();
uint8_t spi_transmit(uint8_t data);
void spi_cs_select();
void spi_cs_unselect();

bool isFirmwareFile(char* fname);

#endif