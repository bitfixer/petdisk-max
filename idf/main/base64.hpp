#pragma once

#include <stdint.h>

int base64_len(int len);
int base64_encode(const uint8_t *src, int len, uint8_t* out);