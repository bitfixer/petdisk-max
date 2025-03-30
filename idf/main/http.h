#pragma once
#include <stdint.h>

namespace HTTP {
    bool request(const char* url, uint8_t* buffer, int size);
}