#ifndef __network_data_source_h__
#define __network_data_source_h__

#include "DataSource.h"
#include "EspHttp.h"
#include "SerialLogger.h"

struct urlData
{
    void* eepromHost;
    int eepromHostLength;
    void* eepromUrl;
    int eepromUrlLength;
};

class NetworkDataSource : public DataSource
{
public:
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
    {
        struct urlInfo* urlInfo = (struct urlInfo*)_dataBuffer;
        _fileName = urlInfo->fileName;
    }
    
    ~NetworkDataSource() {}

    void setUrlData(void* eepromHost, int eepromHostLength, void* eepromUrl, int eepromUrlLength)
    {
        _urlData.eepromHost = eepromHost;
        _urlData.eepromHostLength = eepromHostLength;
        _urlData.eepromUrl = eepromUrl;
        _urlData.eepromUrlLength = eepromUrlLength;
    }

    bool init();
    bool isInitialized();
    void openFileForWriting(unsigned char* fileName);
    bool openFileForReading(unsigned char* fileName);
    bool openDirectory(const char* dirName);
    unsigned int getNextFileBlock();
    bool isLastBlock();
    bool getNextDirectoryEntry();

    void writeBufferToFile(unsigned int numBytes);
    void closeFile();
    void openCurrentDirectory();
    bool isDirectory() { return false; }
    unsigned char* getFilename();
    unsigned char* getBuffer();

    uint32_t seek(uint32_t pos);

    unsigned int readBufferSize()
    {
        return _readBufferSize;
    }

    unsigned int writeBufferSize()
    {
        return 512;
    }

    unsigned int requestReadBufferSize(unsigned int requestedReadBufferSize);
    unsigned int requestWriteBufferSize(unsigned int requestedWriteBufferSize);

private:
    EspHttp* _http;
    uint32_t _fileSize;
    uint32_t _currentBlockByte;
    uint32_t _currentOutputByte;
    int _currentDirectoryPage;
    uint8_t* _blockData;
    uint8_t* _dataBuffer;
    uint16_t* _dataBufferSize;
    //char _fileName[100];
    char* _fileName;
    uint8_t* _dirPtr;
    Logger* _log;
    bool _firstBlockWritten;
    urlData _urlData;
    unsigned int _readBufferSize;
    unsigned int _writeBufferSize;
    
    bool fetchBlock(uint32_t start, uint32_t end);
    void getHost(char* host);
    void getUrl(char* url);
};

#endif