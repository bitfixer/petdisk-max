#ifndef __datasource_h__
#define __datasource_h__

#include <stdint.h>

class DataSource
{
public:
    DataSource() {}
    ~DataSource() {}

    virtual bool init() = 0;
    virtual void openFileForWriting(unsigned char* fileName) = 0;
    virtual bool openFileForReading(unsigned char* fileName) = 0;
    virtual bool openDirectory(const char* dirName) = 0;
    virtual unsigned int getNextFileBlock() = 0;
    virtual bool isLastBlock() = 0;
    virtual bool getNextDirectoryEntry() = 0;
    virtual bool isInitialized() = 0;
    virtual void writeBufferToFile(unsigned int numBytes) = 0;
    virtual void updateBlock() {}
    virtual void closeFile() = 0;
    virtual void openCurrentDirectory() = 0;
    virtual uint32_t seek(uint32_t pos) { return 0; }
    virtual bool isHidden() { return false; }
    virtual bool isVolumeId() { return false; }
    virtual bool isDirectory() { return false; }
    virtual unsigned char* getFilename() = 0;
    virtual unsigned char* getBuffer() = 0;
    virtual unsigned int writeBufferSize() = 0;
    virtual unsigned int readBufferSize() { return 512; }
    virtual unsigned int requestReadBufferSize(unsigned int requestedReadBufferSize) {
        // default to not changing read buffer size
        return 512;
    }
    virtual unsigned int requestWriteBufferSize(unsigned int requestedWriteBufferSize) {
        return 512;
    }

    virtual void processCommandString(int* address) {} // default no action
};

#endif