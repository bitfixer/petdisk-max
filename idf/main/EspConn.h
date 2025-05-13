#ifndef __esp_conn_h__
#define __esp_conn_h__

#include <stdint.h>
#include <stddef.h>

namespace bitfixer
{

class EspConn {
public:
    EspConn()
    : _serialBuffer(NULL)
    , _serialBufferSize(NULL)
    {

    }

    bool initWithParams(uint8_t* buffer, uint16_t* bufferSize);
    bool connect(const char* ssid, const char* passphrase);
    bool startClient(const char* host, uint16_t port);
    void sendData(uint8_t sock, unsigned char* data, int len);
    bool isConnected();
private:

    uint8_t* _serialBuffer;
    uint16_t* _serialBufferSize;

    char _host[256];
    int _port;

    bool _connected = false;

    bool wifi_start();
    bool wifi_stop();
};

}

#endif