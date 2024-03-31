#include "EspHttp.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "hardware.h"
#include "helpers.h"

namespace bitfixer {

void EspHttp::initWithParams(EspConn* espConn, Logger* log)
{
    _espConn = espConn;
    _log = log;
}

bool EspHttp::postBlock(char* host, int port, char* url, char* params, uint8_t* buffer, uint16_t* bufferSize, int numBytes)
{
    // prepare url
    urlInfo* info = (urlInfo*)buffer;
    sprintf_P(info->urlstring, PSTR("PUT %s%s HTTP/1.0\r\nHost: %s\r\nContent-Length: %d\r\n\r\n"), url, params, host, numBytes);

    // move url string to immediately behind payload data
    int url_strlen = strlen(info->urlstring);
    char* startpos = info->blockData - url_strlen;
    memmove(startpos, info->urlstring, url_strlen);

    int full_data_length = url_strlen + numBytes;

    if (!_espConn->startClient(host, port, 0, TCP_MODE))
    {
        // error
        return false;
    }
    _espConn->sendData(0, (unsigned char*)startpos, full_data_length);
    return true;
}

uint8_t* EspHttp::makeRequest(const char* host, int port, const char* url, const char* params, uint8_t* buffer, uint16_t* bufferSize, int* size)
{
    urlInfo* info = (urlInfo*)buffer;
    char* data = info->urlstring;

    sprintf_P(data, PSTR("GET %s%s HTTP/1.0\r\nHost: %s\r\n\r\n"), url, params, host);
    if (!_espConn->startClient(host, port, 0, TCP_MODE))
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
    _log->printf("got size %ld\r\n", fileSize);
    return fileSize;
}

uint8_t* EspHttp::getRange(const char* host, int port, const char* url, uint32_t start, uint32_t end, uint8_t* buffer, uint16_t* bufferSize, int* size)
{
    char params[25];
    sprintf_P(params, PSTR("&s=%" PRIu32 "&e=%" PRIu32), start, end);
    return makeRequest(host, port, url, (const char*)params, buffer, bufferSize, size);
}

}