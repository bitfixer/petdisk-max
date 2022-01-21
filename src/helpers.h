#ifndef __helpers_h__
#define __helpers_h__

#include <stddef.h>
#ifdef ISAVR
// avr
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#else
// esp32
#include <pgmspace.h>
#define sscanf_P sscanf
#endif

#define PACKED __attribute__ ((packed))

const void *bf_memmem(
  const void *haystack, size_t haystacklen, 
  const void *needle,   size_t needlelen );

#endif