#ifndef __network_data_source_h__
#define __network_data_source_h__

#include "DataSource.h"
#include "EspHttp.h"
#include "Serial.h"

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
    NetworkDataSource(EspHttp* http, uint8_t* buffer, uint16_t* bufferSize, Serial1* log) 
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

    unsigned int writeBufferSize()
    {
        return 512;
    }

private:
    EspHttp* _http;
    int _fileSize;
    int _currentBlockByte;
    int _currentOutputByte;
    int _currentDirectoryPage;
    uint8_t* _blockData;
    uint8_t* _dataBuffer;
    uint16_t* _dataBufferSize;
    //char _fileName[100];
    char* _fileName;
    uint8_t* _dirPtr;
    Serial1* _log;
    bool _firstBlockWritten;
    urlData _urlData;
    
    bool fetchBlock(int start, int end);
    void getHost(char* host);
    void getUrl(char* url);
};

#endif