#ifndef __helpers_h__
#define __helpers_h__

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>

#define SPI_CS          PB4

#define PACKED __attribute__ ((packed))
//#define PROGMEM
//#define PSTR(A) A
uint8_t bf_pgm_read_byte(uint8_t* src);

/*
#include <stddef.h>
#include <stdint.h>

#define PSTR(A) A
#define sprintf_P sprintf

#define sscanf_P sscanf



#define PROGMEM 

#define SPI_CS 5

const void *memmem(
  const void *haystack, size_t haystacklen, 
  const void *needle,   size_t needlelen );

uint8_t bf_pgm_read_byte(uint8_t* src);
*/

#endif