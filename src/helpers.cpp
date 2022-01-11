#include "helpers.h"
#include <avr/pgmspace.h>

uint8_t bf_pgm_read_byte(uint8_t* src)
{
  return pgm_read_byte(src);
}