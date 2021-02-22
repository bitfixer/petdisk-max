#include "NetworkDataSource.h"
#include <stdio.h>
#include <string.h>
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

void NetworkDataSource::openFileForWriting(unsigned char* fileName)
{
    // open file for writing
    // blocks of data will be sent to the server.
    // no need to make a request here
    _firstBlockWritten = false;
    urlInfo* info = (urlInfo*)_dataBuffer;
    _blockData = (uint8_t*)info->blockData;
}

bool NetworkDataSource::openFileForReading(unsigned char* fileName)
{
    struct receiveBuffer* recvBuffer = (struct receiveBuffer*)_dataBuffer;

    eeprom_read_block(recvBuffer->receiveUrl, _urlData.eepromUrl, _urlData.eepromUrlLength);
    sprintf_P(&recvBuffer->receiveUrl[_urlData.eepromUrlLength], PSTR("?file=%s"), fileName);

    _log->transmitString(recvBuffer->receiveUrl);
    _log->transmitStringF(PSTR("\r\n"));

    urlInfo* info = (urlInfo*)_dataBuffer;
    eeprom_read_block(info->host, _urlData.eepromHost, _urlData.eepromHostLength);
    info->host[_urlData.eepromHostLength] = 0;

    _fileSize = _http->getSize((const char*)info->host, recvBuffer->receiveUrl, _dataBuffer, _dataBufferSize);
    _currentBlockByte = 0;
    _currentOutputByte = 0;
    _blockData = 0;

    // get first block
    fetchBlock(_currentBlockByte, _currentBlockByte + 512);
    _currentBlockByte += 512;
    return true;
}

bool NetworkDataSource::openDirectory(const char* dirName)
{
    return true;
}

bool NetworkDataSource::fetchBlock(int start, int end)
{
    int rangeSize = 0;
    char temp[128];

    urlInfo* info = (urlInfo*)_dataBuffer;
    eeprom_read_block(info->host, _urlData.eepromHost, _urlData.eepromHostLength);
    info->host[_urlData.eepromHostLength] = 0;

    struct receiveBuffer* recvBuffer = (struct receiveBuffer*)_dataBuffer;

    _log->transmitString(recvBuffer->receiveUrl);
    sprintf_P(temp, PSTR("\r\nhost: %s\r\n"), info->host);
    _log->transmitString(temp);

    _blockData = _http->getRange(
        (const char*)info->host, 
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
    uint32_t q_pos = (pos / readBufferSize()) * readBufferSize();
    _currentOutputByte = q_pos;
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
    int start = _currentOutputByte;
    int end = start + 512;
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
        getHost(info->host);
        getUrl(info->url);
        sprintf_P(info->params, PSTR("?d=1&p=%d"), _currentDirectoryPage++);
        
        _dirPtr = _http->makeRequest(
            (const char*)info->host,
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
    getHost(info->host);
    getUrl(info->url);
    _log->transmitString(info->url);
    _log->transmitStringF(PSTR("\r\n"));

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

void NetworkDataSource::getHost(char* host)
{
    eeprom_read_block(host, _urlData.eepromHost, _urlData.eepromHostLength);
    host[_urlData.eepromHostLength] = 0;
}

void NetworkDataSource::getUrl(char* url)
{
    eeprom_read_block(url, _urlData.eepromUrl, _urlData.eepromUrlLength);
    url[_urlData.eepromUrlLength] = 0;
}