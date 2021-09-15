#include "NetworkDataSource.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>

struct receiveBuffer {
    char serialBuffer[832];
    char receiveUrl[128];
    char fileName[64];
};

bool NetworkDataSource::init()
{
    return true;
}

bool NetworkDataSource::isInitialized()
{
    return true;
}

void NetworkDataSource::copyUrlEscapedString(char* dest, char* src)
{
    int srcLen = strlen(src);
    char* destptr = dest;

    for (int i = 0; i < srcLen; i++)
    {
        char c = src[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
        {
            *destptr++ = c;
        }
        else
        {
            sprintf(destptr, "%%%X", c);
            destptr += 3;
        }
    }
    *destptr = 0x00;
}

void NetworkDataSource::openFileForWriting(unsigned char* fileName)
{
    // open file for writing
    // blocks of data will be sent to the server.
    // no need to make a request here
    _firstBlockWritten = false;
    urlInfo* info = (urlInfo*)_dataBuffer;
    _blockData = (uint8_t*)info->blockData;
    copyUrlEscapedString(_fileName, (char*)fileName);

    _readBufferSize = 512;
    _writeBufferSize = 512;
}

bool NetworkDataSource::openFileForReading(unsigned char* fileName)
{
    struct receiveBuffer* recvBuffer = (struct receiveBuffer*)_dataBuffer;
    copyUrlEscapedString(recvBuffer->fileName, (char*)fileName);

    eeprom_read_block(recvBuffer->receiveUrl, _urlData.eepromUrl, _urlData.eepromUrlLength);
    sprintf_P(&recvBuffer->receiveUrl[_urlData.eepromUrlLength], PSTR("?file=%s"), recvBuffer->fileName);

    urlInfo* info = (urlInfo*)_dataBuffer;
    /*
    eeprom_read_block(info->host, _urlData.eepromHost, _urlData.eepromHostLength);
    info->host[_urlData.eepromHostLength] = 0;

    char* portPtr = strstr(info->host, ":");
    if (portPtr)
    {
        sscanf(portPtr, "%d", &info->port);
        *portPtr = 0;
    }
    else
    {
        info->port = 80;
    }
    */
    getHostAndPort(info->host, &info->port);

    _fileSize = _http->getSize((const char*)info->host, info->port, recvBuffer->receiveUrl, _dataBuffer, _dataBufferSize);
    _log->printf("filesize %ld\r\n", _fileSize);
    _currentBlockByte = 0;
    _currentOutputByte = 0;
    _blockData = 0;

    _readBufferSize = 512;
    _writeBufferSize = 512;

    if (_fileSize <= 0)
    {
        return false;
    }

    // get first block
    fetchBlock(_currentBlockByte, _currentBlockByte + _readBufferSize);
    _currentBlockByte += _readBufferSize;
    return true;
}

bool NetworkDataSource::openDirectory(const char* dirName)
{
    return true;
}

unsigned int NetworkDataSource::requestReadBufferSize(unsigned int requestedReadBufferSize)
{
    _readBufferSize = requestedReadBufferSize;
    return _readBufferSize;
}

unsigned int NetworkDataSource::requestWriteBufferSize(unsigned int requestedWriteBufferSize)
{
    _writeBufferSize = requestedWriteBufferSize;
    return _writeBufferSize;
}

bool NetworkDataSource::fetchBlock(uint32_t start, uint32_t end)
{
    int rangeSize = 0;

    urlInfo* info = (urlInfo*)_dataBuffer;
    //eeprom_read_block(info->host, _urlData.eepromHost, _urlData.eepromHostLength);
    //info->host[_urlData.eepromHostLength] = 0;

    getHostAndPort(info->host, &info->port);

    struct receiveBuffer* recvBuffer = (struct receiveBuffer*)_dataBuffer;

    _blockData = _http->getRange(
        (const char*)info->host, 
        info->port,
        (const char*)recvBuffer->receiveUrl, 
        start, 
        end, 
        _dataBuffer, 
        _dataBufferSize, 
        &rangeSize);

     return true;
}

uint32_t NetworkDataSource::seek(uint32_t pos)
{
    // round to closest block
    uint32_t q_pos = (pos / (uint32_t)readBufferSize()) * (uint32_t)readBufferSize();
    _currentOutputByte = q_pos;
    _currentBlockByte = 0;
    return _currentOutputByte;
}

unsigned int NetworkDataSource::getNextFileBlock()
{
    if (_currentOutputByte < _currentBlockByte)
    {
        unsigned int bytes = _currentBlockByte - _currentOutputByte;
        _currentOutputByte = _currentBlockByte;
        return bytes;
    }

    // retrieve range from server
    uint32_t start = _currentOutputByte;
    uint32_t end = start + _readBufferSize;
    
    if (end > _fileSize + 1)
    {
        end = _fileSize + 1;
    }

    fetchBlock(start, end);

    if (_blockData)
    {
        _currentBlockByte = end;
        _currentOutputByte = _currentBlockByte;
        return end-start;
    }

    return 0;
}

bool NetworkDataSource::isLastBlock()
{
    if (_currentOutputByte >= _fileSize)
    {
        return true;
    }

    return false;
}

bool NetworkDataSource::getNextDirectoryEntry()
{
    // if there is no directory information
    // or if we have reached the end of this page,
    // fetch the next page from the server.
    if (_dirPtr == 0 || _dirPtr[0] == '\n')
    {
        int size;
        // prepare address
        urlInfo* info = (urlInfo*)_dataBuffer;
        getHostAndPort(info->host, &info->port);
        getUrl(info->url);
        sprintf_P(info->params, PSTR("?d=1&p=%d"), _currentDirectoryPage++);
        
        _dirPtr = _http->makeRequest(
            (const char*)info->host,
            info->port,
            (const char*)info->url,
            (const char*)info->params,
            _dataBuffer,
            _dataBufferSize,
            &size);
    }

    if (_dirPtr == 0)
    {
        // no directory
        return false;
    }

    if (_dirPtr[0] == '\n')
    {
        // if the first character is newline, this is the end of the directory.
        _dirPtr = 0;
        return false;
    }

    return true;
}

void NetworkDataSource::writeBufferToFile(unsigned int numBytes)
{
    // data is located in _dataBuffer
    // make request, parameters
    // prepare address
    urlInfo* info = (urlInfo*)_dataBuffer;

    // specify url parameters
    getHostAndPort(info->host, &info->port);
    getUrl(info->url);
    
    if (_firstBlockWritten == false)
    {
        sprintf_P(info->params, PSTR("?f=%s&n=1"), _fileName);
        _firstBlockWritten = true;
    }
    else
    {
        sprintf_P(info->params, PSTR("?f=%s"), _fileName);
    }

    _http->postBlock(
        info->host,
        info->url,
        info->params,
        _dataBuffer,
        _dataBufferSize,
        numBytes);
}

void NetworkDataSource::updateBlock()
{
    // write specific block to file
    // data is located in _dataBuffer
    // make request, parameters
    // prepare address
    urlInfo* info = (urlInfo*)_dataBuffer;
    memmove(info->blockData, _blockData, _writeBufferSize);

    // specify url parameters
    getHostAndPort(info->host, &info->port);
    getUrl(info->url);

    uint32_t endByte = _currentOutputByte + _writeBufferSize;
    struct receiveBuffer* recvBuffer = (struct receiveBuffer*)_dataBuffer;
    sprintf_P(info->params, PSTR("?f=%s&u=1&s=%ld&e=%ld"), recvBuffer->fileName, _currentOutputByte, endByte);

    _http->postBlock(
        info->host,
        info->url,
        info->params,
        _dataBuffer,
        _dataBufferSize,
        _writeBufferSize);
}

void NetworkDataSource::closeFile()
{
    // close the file,
    // no need for additional request
}

void NetworkDataSource::openCurrentDirectory()
{
    _dirPtr = 0;
    _currentDirectoryPage = 0;
}

unsigned char* NetworkDataSource::getFilename()
{
    uint8_t* thisFilename = _dirPtr;
    // find end of this filename
    while (*_dirPtr != '\n')
    {
        _dirPtr++;
    }

    // set null terminator and advance to next filename
    _dirPtr[0] = 0;
    _dirPtr++;

    return thisFilename;
}

unsigned char* NetworkDataSource::getBuffer()
{
    return _blockData;
}

void NetworkDataSource::getHostAndPort(char* host, uint16_t* port)
{
    eeprom_read_block(host, _urlData.eepromHost, _urlData.eepromHostLength);
    host[_urlData.eepromHostLength] = 0;

    _log->log("**host: ");
    _log->log(host);
    _log->log("\r\n");

    char* portPtr = strstr(host, ":");
    if (portPtr)
    {
        *portPtr = 0;
        portPtr++;
        _log->printf("portstr: %s\r\n", portPtr);
        sscanf(portPtr, "%d", port);
        *portPtr = 0;
    }
    else
    {
        *port = 80;
    }
}

void NetworkDataSource::getUrl(char* url)
{
    eeprom_read_block(url, _urlData.eepromUrl, _urlData.eepromUrlLength);
    url[_urlData.eepromUrlLength] = 0;
}