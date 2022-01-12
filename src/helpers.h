#ifndef __helpers_h__
#define __helpers_h__

#include <stddef.h>
//#include <avr/interrupt.h>
//#include <avr/pgmspace.h>

#define PSTR(A) A
#define PROGMEM
#define sprintf_P sprintf
#define sscanf_P sscanf

#define PACKED __attribute__ ((packed))

const void *bf_memmem(
  const void *haystack, size_t haystacklen, 
  const void *needle,   size_t needlelen );

#endif