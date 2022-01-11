#ifndef __esp_conn_h__
#define __esp_conn_h__

#include <stdint.h>
#include "SerialLogger.h"
#include "Serial.h"

namespace bitfixer
{

typedef enum eProtMode {TCP_MODE, UDP_MODE, SSL_MODE} tProtMode;
typedef enum
{
    TAG_OK,
    TAG_ERROR,
    TAG_FAIL,
    TAG_SENDOK,
    TAG_CONNECT
} TagsEnum;

class EspConn {
public:
    EspConn()
    : _serialBuffer(NULL)
    , _serialBufferSize(NULL)
    , _serial(NULL)
    , _logSerial(NULL)
    {

    }

    EspConn(uint8_t* buffer, uint16_t* bufferSize, Serial* serial, Logger* logSerial) : 
    _serialBuffer(buffer),
    _serialBufferSize(bufferSize),
    _serial(serial),
    _logSerial(logSerial)
    {

    }

    bool initWithParams(uint8_t* buffer, uint16_t* bufferSize, Serial* serial, Logger* logSerial);
    bool init();
    bool device_present();
    bool attempt_baud_rate_setting();

    void scanNetworks();

    void setDns();
    bool connect(const char* ssid, const char* passphrase);
    bool startClient(const char* host, uint16_t port, uint8_t sock, uint8_t protMode);
    void stopClient(uint8_t sock);

    void sendData(uint8_t sock, unsigned char* data, int len);
    int readUntil(const char* tag, bool findTags, bool end, int timeout);
    bool sendCmd(const char* cmd, int timeout=30000);
    void reset();

private:
    
    void readBytesUntilSize(uint16_t size);
    void copyEscapedString(char* dest, const char* src);

    uint8_t* _serialBuffer;
    uint16_t* _serialBufferSize;
    Serial* _serial;
    Logger* _logSerial;
};

}

#endif