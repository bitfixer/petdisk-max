#ifndef __esp_http_h__
#define __esp_http_h__

#include "EspConn.h"
#include <stdint.h>
#include "SerialLogger.h"

struct urlInfo {
    char host[62];
    uint16_t port;
    char url[64];
    char params[64];
    char urlstring[256];
    char blockData[512];
    char fileName[64];
};

class EspHttp 
{
public:
    EspHttp(EspConn* espConn, Logger* log) 
    : _espConn(espConn)
    , _log(log)
    {}

    ~EspHttp() {}

    bool postBlock(char* host, char* url, char* params, uint8_t* buffer, uint16_t* bufferSize, int numBytes);
    uint8_t* makeRequest(const char* host, uint16_t port, const char* url, const char* params, uint8_t* buffer, uint16_t* bufferSize, int* size);
    uint32_t getSize(const char* host, uint16_t port, const char* url, uint8_t* buffer, uint16_t* bufferSize);
    uint8_t* getRange(const char* host, uint16_t port, const char* url, uint32_t start, uint32_t end, uint8_t* buffer, uint16_t* bufferSize, int* size);

    int getSizeE(const char* host, const char* url, uint8_t* buffer, uint16_t* bufferSize);

private:
    EspConn* _espConn;
    Logger* _log;
};

#endif