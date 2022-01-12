#include "EspConn.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

namespace bitfixer 
{

WiFiClient client;
HTTPClient httpClient;

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
    return true;
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
    return true;
    // todo: handle connection failure
}

bool EspConn::sendCmd(const char* cmd, int timeout) {
    return true;
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
    client.write(data, len);
    int recCount = 0;
    while (client.available() || client.connected())
    {
        int byte = client.read();
        if (byte != -1)
        {
            _serialBuffer[recCount] = byte;
            recCount++;
        }
    }

    *_serialBufferSize = recCount;
}

void EspConn::readBytesUntilSize(uint16_t size)
{
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
   return -1;
}

void EspConn::reset() {
}

}