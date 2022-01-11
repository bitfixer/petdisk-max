#ifndef __datasource_h__
#define __datasource_h__

#include <stdint.h>

class DataSource
{
public:
    DataSource() {}
    ~DataSource() {}

    virtual bool init() = 0;
    virtual void openFileForWriting(uint8_t* fileName) = 0;
    virtual bool openFileForReading(uint8_t* fileName) = 0;
    virtual bool openDirectory(const char* dirName) = 0;
    virtual uint16_t getNextFileBlock() = 0;
    virtual bool isLastBlock() = 0;
    virtual bool getNextDirectoryEntry() = 0;
    virtual bool isInitialized() = 0;
    virtual void writeBufferToFile(uint16_t numBytes) = 0;
    virtual void updateBlock() {}
    virtual void closeFile() = 0;
    virtual void openCurrentDirectory() = 0;
    virtual uint32_t seek(uint32_t pos) { return 0; }
    virtual bool isHidden() { return false; }
    virtual bool isVolumeId() { return false; }
    virtual bool isDirectory() { return false; }
    virtual uint8_t* getFilename() = 0;
    virtual uint8_t* getBuffer() = 0;
    virtual uint16_t writeBufferSize() = 0;
    virtual uint16_t readBufferSize() { return 512; }
    virtual uint16_t requestReadBufferSize(uint16_t requestedReadBufferSize) {
        // default to not changing read buffer size
        return 512;
    }
    virtual uint16_t requestWriteBufferSize(uint16_t requestedWriteBufferSize) {
        return 512;
    }

    virtual void processCommandString(int* address) {} // default no action
};

#endif