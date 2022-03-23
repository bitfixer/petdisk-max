#include "helpers.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

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

void lowerStringInPlace(char* str)
{
    char* c = str;
    while (*c != 0)
    {
        *c = tolower(*c);
        c++;
    }
}

void upperStringInPlace(char* str)
{
    char* c = str;
    while (*c != 0)
    {
        *c = toupper(*c);
        c++;
    }
}