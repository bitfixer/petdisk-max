#ifndef __esp_http_h__
#define __esp_http_h__

#include "EspConn.h"
#include <stdint.h>
#include "Serial.h"

struct urlInfo {
    char host[64];
    char url[64];
    char params[64];
    char urlstring[256];
    char blockData[512];
    char fileName[64];
};

class EspHttp 
{
public:
    EspHttp(EspConn* espConn, Serial1* log) 
    : _espConn(espConn)
    , _log(log)
    {}

    ~EspHttp() {}

    bool postBlock(char* host, char* url, char* params, uint8_t* buffer, uint16_t* bufferSize, int numBytes);
    uint8_t* makeRequest(const char* host, const char* url, const char* params, uint8_t* buffer, uint16_t* bufferSize, int* size);
    int getSize(const char* host, const char* url, uint8_t* buffer, uint16_t* bufferSize);
    uint8_t* getRange(const char* host, const char* url, int start, int end, uint8_t* buffer, uint16_t* bufferSize, int* size);

    int getSizeE(const char* host, const char* url, uint8_t* buffer, uint16_t* bufferSize);

private:
    EspConn* _espConn;
    Serial1* _log;
};

#endif