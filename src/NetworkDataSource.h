#ifndef __network_data_source_h__
#define __network_data_source_h__

#include "DataSource.h"
#include "EspHttp.h"
#include "SerialLogger.h"
#include "Settings.h"

namespace bitfixer {

class NetworkDataSource : public DataSource
{
public:
    NetworkDataSource()
    : _http(NULL)
    , _fileSize(0)
    , _currentBlockByte(0)
    , _currentOutputByte(0)
    , _currentDirectoryPage(0)
    , _blockData(0)
    , _dataBuffer(NULL)
    , _dataBufferSize(NULL)
    , _dirPtr(0)
    , _log(NULL)
    , _firstBlockWritten(false)
    , _readBufferSize(512)
    , _writeBufferSize(512)
     {}

    NetworkDataSource(EspHttp* http, uint8_t* buffer, uint16_t* bufferSize, Logger* log) 
    : _http(http)
    , _fileSize(0)
    , _currentBlockByte(0)
    , _currentOutputByte(0)
    , _currentDirectoryPage(0)
    , _blockData(0)
    , _dataBuffer(buffer)
    , _dataBufferSize(bufferSize)
    , _dirPtr(0)
    , _log(log)
    , _firstBlockWritten(false)
    , _readBufferSize(512)
    , _writeBufferSize(512)
    {
        struct urlInfo* urlInfo = (struct urlInfo*)_dataBuffer;
        _fileName = urlInfo->fileName;
    }
    
    ~NetworkDataSource() {}

    void initWithParams(EspHttp* http, uint8_t* buffer, uint16_t* bufferSize, Logger* log)
    {
        _http = http;
        _dataBuffer = buffer;
        _dataBufferSize = bufferSize;
        _log = log;

        struct urlInfo* urlInfo = (struct urlInfo*)_dataBuffer;
        _fileName = urlInfo->fileName;
    }

    void setUrlData(void* eepromHost, int eepromHostLength, int port, void* eepromUrl, int eepromUrlLength)
    {
        _settings.initWithParams(eepromHost, eepromHostLength, port, eepromUrl, eepromUrlLength);
    }

    bool init();
    bool isInitialized();
    void openFileForWriting(unsigned char* fileName);
    bool openFileForReading(unsigned char* fileName);
    bool openDirectory(const char* dirName);
    uint16_t getNextFileBlock();
    bool isLastBlock();
    bool getNextDirectoryEntry();

    void writeBufferToFile(uint16_t numBytes);
    void updateBlock();
    void closeFile();
    void openCurrentDirectory();
    bool isDirectory() { return false; }
    unsigned char* getFilename();
    unsigned char* getBuffer();

    bool getCurrentDateTime(int* year, int* month, int* day, int* hour, int* minute, int* second);

    uint32_t seek(uint32_t pos);

    uint16_t readBufferSize()
    {
        return _readBufferSize;
    }

    uint16_t writeBufferSize()
    {
        return _writeBufferSize;
    }

    uint16_t requestReadBufferSize(uint16_t requestedReadBufferSize);
    uint16_t requestWriteBufferSize(uint16_t requestedWriteBufferSize);

    Settings _settings;
private:
    EspHttp* _http;
    uint32_t _fileSize;
    uint32_t _currentBlockByte;
    uint32_t _currentOutputByte;
    int _currentDirectoryPage;
    uint8_t* _blockData;
    uint8_t* _dataBuffer;
    uint16_t* _dataBufferSize;
    char* _fileName;
    uint8_t* _dirPtr;
    Logger* _log;
    bool _firstBlockWritten;
    
    uint16_t _readBufferSize;
    uint16_t _writeBufferSize;
    
    bool fetchBlock(uint32_t start, uint32_t end);
    void copyUrlEscapedString(char* dest, char* src);
};

}

#endif