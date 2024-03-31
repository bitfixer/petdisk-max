#include "EspHttp.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "hardware.h"
#include "helpers.h"
#include "base64.hpp"

namespace bitfixer {

void EspHttp::initWithParams(EspConn* espConn)
{
    _espConn = espConn;
}

bool EspHttp::postBlock(char* host, int port, char* url, char* params, uint8_t* buffer, uint16_t* bufferSize, int numBytes)
{
    int encoded_len = base64_len(numBytes);
    urlInfo* info = (urlInfo*)buffer;
    sprintf((char*)sendBuffer, "PUT %s%s&b64=1 HTTP/1.0\r\nHost: %s\r\nContent-Length: %d\r\n\r\n", url, params, host, encoded_len);
    int s = strlen((const char*)sendBuffer);
    uint8_t* dataStart = &sendBuffer[s];
    int full_data_length = s + base64_encode((uint8_t*)info->blockData, numBytes, dataStart);

    if (!_espConn->startClient(host, port))
    {
        // error
        return false;
    }
    _espConn->sendData(0, sendBuffer, full_data_length);
    return true;
}

//#define REQUEST_HASH 1

#ifdef REQUEST_HASH
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                       +(uint32_t)(((const uint8_t *)(d))[0]) )

uint32_t SuperFastHash (const char * data, int len) {
uint32_t hash = len, tmp;
int rem;

    if (len <= 0 || data == NULL) return 0;

    rem = len & 3;
    len >>= 2;

    /* Main loop */
    for (;len > 0; len--) {
        hash  += get16bits (data);
        tmp    = (get16bits (data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        data  += 2*sizeof (uint16_t);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
        case 3: hash += get16bits (data);
                hash ^= hash << 16;
                hash ^= ((signed char)data[sizeof (uint16_t)]) << 18;
                hash += hash >> 11;
                break;
        case 2: hash += get16bits (data);
                hash ^= hash << 11;
                hash += hash >> 17;
                break;
        case 1: hash += (signed char)*data;
                hash ^= hash << 10;
                hash += hash >> 1;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}
#endif

uint8_t* EspHttp::makeRequest(const char* host, int port, const char* url, const char* params, uint8_t* buffer, uint16_t* bufferSize, int* size)
{
    urlInfo* info = (urlInfo*)buffer;
    char* data = info->urlstring;

    sprintf_P(data, PSTR("GET %s%s HTTP/1.0\r\nHost: %s\r\n\r\n"), url, params, host);
    if (!_espConn->startClient(host, port))
    {
        return NULL;
    }
    _espConn->sendData(0, (unsigned char*)data, strlen(data));
    int bufSize = *bufferSize;
    
    // parse data from serial buffer
    // find beginning of HTTP message
    uint8_t* httpStart = (uint8_t*)bf_memmem(buffer, bufSize, "HTTP", 4);
    if (httpStart == 0)
    {
        *size = 0;
        return 0;
    }

    int httpSize = bufSize - (httpStart - buffer);

    // find blank newline to signify beginning of payload data
    uint8_t* datastart = (uint8_t*)bf_memmem(httpStart, httpSize, "\r\n\r\n", 4);

    if (datastart == 0)
    {
        *size = -1;
        return 0;
    }

    datastart += 4;
    *size = bufSize - (datastart - buffer);

    #ifdef REQUEST_HASH
    uint32_t hash = SuperFastHash((const char*)datastart, *size);
    log_i("hash %04lX", hash);
    #endif

    return datastart;
}

uint32_t EspHttp::getSize(const char* host, int port, const char* url, uint8_t* buffer, uint16_t* bufferSize)
{
    uint8_t* datastart;
    uint32_t fileSize;
    int size;
    datastart = makeRequest(host, port, url, "&l=1", buffer, bufferSize, &size);
    if (datastart == NULL)
    {
        return 0;
    }

    sscanf((const char*)datastart, "%" SCNu32 "\r\n", &fileSize);
    return fileSize;
}

uint8_t* EspHttp::getRange(const char* host, int port, const char* url, uint32_t start, uint32_t end, uint8_t* buffer, uint16_t* bufferSize, int* size)
{
    char params[25];
    sprintf_P(params, PSTR("&s=%" PRIu32 "&e=%" PRIu32), start, end);
    return makeRequest(host, port, url, (const char*)params, buffer, bufferSize, size);
}

}