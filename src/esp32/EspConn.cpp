#include "EspConn.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define NUMESPTAGS 5

namespace bitfixer 
{

WiFiClient client;
HTTPClient httpClient;

const char* ESPTAGS[] =
{
    "\r\nOK\r\n",
    "\r\nERROR\r\n",
    "\r\nFAIL\r\n",
    "\r\nSEND OK\r\n",
    " CONNECT\r\n"
};

typedef enum
{
    STATE_READ = 0,
    STATE_O = 1,
    STATE_K = 2,
    STATE_NL_1 = 3,
    STATE_NL_2 = 4,
} readState;

bool EspConn::device_present() {
    // always present for esp32 device
    return true;
}

bool EspConn::attempt_baud_rate_setting() {
    // not applicable
    return true;
}

bool EspConn::initWithParams(uint8_t* buffer, uint16_t* bufferSize, Serial* serial, Logger* logSerial)
{
    _serialBuffer = buffer;
    _serialBufferSize = bufferSize;
    _serial = serial;
    _logSerial = logSerial;
    init();
}
    
bool EspConn::init() {
    // not applicable
    return true;
}

void EspConn::scanNetworks() {
    int n = WiFi.scanNetworks();
    _logSerial->printf("found %d networks.\n", n);
    for (int i = 0; i < n; i++)
    {
    _logSerial->printf("%d: %s %d\n", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    }
}

void EspConn::setDns() {
    // not applicable
}

void EspConn::copyEscapedString(char* dest, const char* src)
{
    int srcLen = strlen(src);
    char* destptr = dest;

    for (int i = 0; i < srcLen; i++)
    {
        char c = src[i];
        if (isalnum(c))
        {
            *destptr++ = c;
        }
        else
        {
            sprintf(destptr, "\\%c", c);
            destptr += 2;
        }
    }
    destptr = 0x00;
}

bool EspConn::connect(const char* ssid, const char* passphrase) {
     WiFi.begin(ssid, passphrase);
     while (WiFi.status() != WL_CONNECTED) {
         delay(500);
         _logSerial->printf(".");
     }
    _logSerial->printf("wifi connected\n");
}

bool EspConn::sendCmd(const char* cmd, int timeout) {
    /*
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        *_serialBufferSize = 0;
    }

    _serial->transmitString(cmd);
    _serial->enable_interrupt();
    _serial->transmitString("\r\n");
    int result = readUntil(0, true, true, timeout);
    _serial->disable_interrupt();

    if (result < 0 || result == 2) // FAIL
    {
        return false;
    }
    
    return true;
    */
}

bool EspConn::startClient(const char* host, uint16_t port, uint8_t sock, uint8_t protMode)
{
    int ret = client.connect(host, port);
    if (ret == 1)
    {
        return true;
    }

    return false;
}

void EspConn::stopClient(uint8_t sock)
{
}


int contains(const char* big, const char* small, size_t size)
{
    int sm_len = strlen(small);
    int end = size - sm_len;
    for (int i = 0; i <= end; i++)
    {
        if (big[i] == small[0])
        {
            if (memcmp(&big[i], small, sm_len) == 0)
            {
                return i;
            }
        }
    }

    return -1;
}

void EspConn::sendData(uint8_t sock, unsigned char* data, int len)
{
    //_logSerial->printf("esp send data length %d: %s\n", len, (char*)data);
    int bytesWritten = client.write(data, len);
    //_logSerial->printf("sent %d\n", bytesWritten);
    
    int recCount = 0;
    while (client.available() || client.connected())
    {
        int byte = client.read();
        if (byte != -1)
        {
            //_logSerial->printf("got %d: %X %c\n", recCount, byte, byte);
            _serialBuffer[recCount] = byte;
            //_logSerial->printf("%c", byte);
            recCount++;
        }
    }

    *_serialBufferSize = recCount;
}

void EspConn::readBytesUntilSize(uint16_t size)
{
    /*
    // keep reading until the right number of bytes has been read
    bool done = false;
    uint16_t current_buffer_size = 0;
    while (!done)
    {
        ATOMIC_BLOCK(ATOMIC_FORCEON) 
        {
            current_buffer_size = *_serialBufferSize;
        }

        if (current_buffer_size >= size)
        {
            done = true;
        }
        else
        {
            _delay_ms(1);
        }
    }
    */
}

bool endsWith(const char* big, const char* small, size_t size)
{
    int start = size - strlen(small);
    for (int i = 0; i < (int)strlen(small); i++)
    {
        if (big[i+start] != small[i])
        {
            return false;
        } 
    }

    return true;
}

int EspConn::readUntil(const char* tag, bool findTags, bool end, int timeout) {
    /*
    int ret = -1;
    uint16_t current_buffer_size = 0;
    int currtime = 0;
    while (ret < 0 && currtime < timeout) 
    {
        ATOMIC_BLOCK(ATOMIC_FORCEON) {
            current_buffer_size = *_serialBufferSize;
        }
        if (current_buffer_size > 0) 
        {
            if (tag != 0) 
            {
                if (end)
                {
                    if (endsWith((const char*)_serialBuffer, tag, current_buffer_size) == true) 
                    {
                        ret = 1;
                        break;
                    }
                }
                else
                {
                    if (contains((const char*)_serialBuffer, tag, current_buffer_size) > 0)
                    {
                        ret = 1;
                        break;
                    }
                }
                //_delay_ms(1);
            }
            else 
            {
                for(int i = 0; i < NUMESPTAGS; i++)
                {
                    if (endsWith((const char*)_serialBuffer, ESPTAGS[i], current_buffer_size) == true)
                    {
                        ret = i;
                        break;
                    }
                }
            }
        }
        currtime++;
        _delay_ms(1);
    }

    return ret;
    */
   return -1;
}

void EspConn::reset() {
}

}