#ifndef __esp_conn_h__
#define __esp_conn_h__

#include <stdint.h>
#include "Serial.h"

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
    EspConn(uint8_t* buffer, uint16_t* bufferSize, Serial* serial, Serial1* logSerial) : 
    _serialBuffer(buffer),
    _serialBufferSize(bufferSize),
    _serial(serial),
    _logSerial(logSerial)
    {

    }

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

    uint8_t* _serialBuffer;
    uint16_t* _serialBufferSize;
    Serial* _serial;
    Serial1* _logSerial;
};

#endif