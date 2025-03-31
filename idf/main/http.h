#pragma once
#include <stdint.h>

namespace HTTP {
    int request(const char* url, uint8_t* buffer, int size);
}