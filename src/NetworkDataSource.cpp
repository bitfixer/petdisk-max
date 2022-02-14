#include "NetworkDataSource.h"
#include "hardware.h"
#include "Settings.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>

struct __attribute__ ((packed)) receiveBuffer {
    char serialBuffer[832];
    char receiveUrl[128];
    char fileName[64];
};

namespace bitfixer {

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

    int receiveUrlLength = _settings.getUrl(recvBuffer->receiveUrl);
    sprintf_P(&recvBuffer->receiveUrl[receiveUrlLength], PSTR("?file=%s"), recvBuffer->fileName);
    
    urlInfo* info = (urlInfo*)_dataBuffer;
    _settings.getHost(info->host);
    int port = _settings.getPort();
    
    _fileSize = _http->getSize((const char*)info->host, port, recvBuffer->receiveUrl, _dataBuffer, _dataBufferSize);
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

    //delay(1000);
    return true;
}

bool NetworkDataSource::openDirectory(const char* dirName)
{
    return true;
}

uint16_t NetworkDataSource::requestReadBufferSize(uint16_t requestedReadBufferSize)
{
    _readBufferSize = requestedReadBufferSize;
    return _readBufferSize;
}

uint16_t NetworkDataSource::requestWriteBufferSize(uint16_t requestedWriteBufferSize)
{
    _writeBufferSize = requestedWriteBufferSize;
    return _writeBufferSize;
}

bool NetworkDataSource::fetchBlock(uint32_t start, uint32_t end)
{
    int rangeSize = 0;

    urlInfo* info = (urlInfo*)_dataBuffer;
    int hostLength = _settings.getHost(info->host);
    int port = _settings.getPort();

    info->host[hostLength] = 0;

    struct receiveBuffer* recvBuffer = (struct receiveBuffer*)_dataBuffer;

    _blockData = _http->getRange(
        (const char*)info->host,
        port,
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

uint16_t NetworkDataSource::getNextFileBlock()
{
    if (_currentOutputByte < _currentBlockByte)
    {
        uint16_t bytes = _currentBlockByte - _currentOutputByte;
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
        _settings.getHost(info->host);
        _settings.getUrl(info->url);
        int port = _settings.getPort();

        sprintf_P(info->params, PSTR("?d=1&p=%d"), _currentDirectoryPage++);
        
        _dirPtr = _http->makeRequest(
            (const char*)info->host,
            port,
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

void NetworkDataSource::writeBufferToFile(uint16_t numBytes)
{
    // data is located in _dataBuffer
    // make request, parameters
    // prepare address
    urlInfo* info = (urlInfo*)_dataBuffer;

    // specify url parameters
    _settings.getHost(info->host);
    _settings.getUrl(info->url);
    int port = _settings.getPort();

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
        port,
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
    _settings.getHost(info->host);
    _settings.getUrl(info->url);
    int port = _settings.getPort();

    uint32_t endByte = _currentOutputByte + _writeBufferSize;
    struct receiveBuffer* recvBuffer = (struct receiveBuffer*)_dataBuffer;
    sprintf_P(info->params, PSTR("?f=%s&u=1&s=%" PRIu32 "&e=%" PRIu32), recvBuffer->fileName, _currentOutputByte, endByte);

    _http->postBlock(
        info->host,
        port,
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

bool NetworkDataSource::getCurrentDateTime(int* year, int* month, int* day, int* hour, int* minute, int* second)
{
    // try to fetch time
    if (openFileForReading((uint8_t*)"TIME"))
    {
        _log->printf("got time\n");
        uint16_t blockSize = getNextFileBlock();
        _log->printf("size: %d\n", blockSize);

        //int timestamp = 0;
        //sscanf((const char*)_petdisk._timeDataSource->getBuffer(), "%d", &timestamp);
        //_logger.printf("timestamp is %d\n", timestamp);

        if (blockSize >= _fileSize) {
            // read in current date and time from string
            sscanf((const char*)getBuffer(), "%d-%d-%d %d:%d:%d\n", year, month, day, hour, minute, second);
            return true;
        }
    }

    // somehow failed
    return false;
}

}