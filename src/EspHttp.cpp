#include "EspHttp.h"
#include <stdio.h>
#include <string.h>
#include <avr/pgmspace.h>

bool EspHttp::postBlock(char* host, char* url, char* params, uint8_t* buffer, uint16_t* bufferSize, int numBytes)
{
    _log->transmitString(params);
    _log->transmitStringF(PSTR("\r\n"));
    // prepare url
    urlInfo* info = (urlInfo*)buffer;
    sprintf_P(info->urlstring, PSTR("PUT %s%s HTTP/1.0\r\nHost: %s\r\nContent-Length: %d\r\n\r\n"), url, params, host, numBytes);

    _log->transmitString(info->urlstring);
    _log->transmitStringF(PSTR("\r\n"));

    // move url string to immediately behind payload data
    int url_strlen = strlen(info->urlstring);
    char* startpos = info->blockData - url_strlen;
    memmove(startpos, info->urlstring, url_strlen);

    int full_data_length = url_strlen + numBytes;

    _espConn->startClient(host, 80, 0, TCP_MODE);

    _espConn->sendData(0, (unsigned char*)startpos, full_data_length);
    //int bufSize = *bufferSize;
    //_espConn->stopClient(0);

    return true;
}

uint8_t* EspHttp::makeRequest(const char* host, const char* url, const char* params, uint8_t* buffer, uint16_t* bufferSize, int* size)
{
    urlInfo* info = (urlInfo*)buffer;
    char* data = info->urlstring;

    sprintf_P(data, PSTR("GET %s%s HTTP/1.0\r\nHost: %s\r\n\r\n"), url, params, host);
    _espConn->startClient(host, 80, 0, TCP_MODE);
    _log->transmitString(data);
    int bytes = _espConn->sendData(0, (unsigned char*)data, strlen(data));
    int bufSize = *bufferSize;
    //_espConn->stopClient(0);

    // parse data from serial buffer
    // find beginning of HTTP message
    uint8_t* httpStart = (uint8_t*)memmem(buffer, bufSize, "HTTP", 4);
    if (httpStart == 0)
    {
        *size = 0;
        return 0;
    }

    // find blank newline to signify beginning of payload data
    uint8_t* datastart = (uint8_t*)memmem(httpStart, bytes, "\r\n\r\n", 4);

    if (datastart == 0)
    {
        *size = -1;
        return 0;
    }

    datastart += 4;
    *size = bufSize - (datastart - buffer);
    return datastart;
}

int EspHttp::getSize(const char* host, const char* url, uint8_t* buffer, uint16_t* bufferSize)
{
    uint8_t* datastart;
    int fileSize;
    int size;
    datastart = makeRequest(host, url, "&l=1", buffer, bufferSize, &size);
    sscanf((const char*)datastart, "%d\r\n", &fileSize);
    return fileSize;
}

uint8_t* EspHttp::getRange(const char* host, const char* url, int start, int end, uint8_t* buffer, uint16_t* bufferSize, int* size)
{
    char params[25];
    sprintf_P(params, PSTR("&s=%d&e=%d"), start, end);
    _log->transmitString(params);
    _log->transmitStringF(PSTR("\r\n"));
    return makeRequest(host, url, (const char*)params, buffer, bufferSize, size);
}