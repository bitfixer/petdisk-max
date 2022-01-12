#include "helpers.h"
#include <stdio.h>
#include <string.h>

const void *bf_memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen)
{
    uint8_t* hh = (uint8_t*)haystack;
    uint8_t* nn = (uint8_t*)needle;

    //uint32_t bytesToCheck = haystacklen - needlelen - 1;
    int last = haystacklen - needlelen + 1;
    for (int i = 0; i < last; i++)
    {
        if (hh[i] == nn[0])
        {
            // compare bytes of haystack and needle
            if (memcmp(&hh[i], nn, needlelen) == 0)
            {
                return &hh[i];
            }
        }
    }

    return NULL;
}