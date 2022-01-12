#ifndef __hardware_h__
#define __hardware_h__

#include <stdint.h>
#include <stddef.h>

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

#endif