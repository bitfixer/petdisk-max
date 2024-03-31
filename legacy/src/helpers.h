#ifndef __helpers_h__
#define __helpers_h__

#include <stddef.h>
#define PACKED __attribute__ ((packed))

const void *bf_memmem(
  const void *haystack, size_t haystacklen, 
  const void *needle,   size_t needlelen );

void lowerStringInPlace(char* str);
void upperStringInPlace(char* str);

#endif