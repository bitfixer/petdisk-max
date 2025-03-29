#pragma once

#include "DataSource.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

namespace bitfixer {

class HTTPDataSource : public DataSource {
public:
    HTTPDataSource() {
        _buffer = (uint8_t*)malloc(512);
        _file_buffer = (uint8_t*)malloc(64*1024);
    }
    ~HTTPDataSource() {
        if (_buffer) {
            free(_buffer);
        }

        if (_file_buffer) {
            free(_file_buffer);
        }
    }

    virtual bool init() override {}
    virtual void openFileForWriting(uint8_t* fileName) override {}
    virtual bool openFileForReading(uint8_t* fileName) override;
    virtual bool openDirectory(const char* dirName) override {}
    virtual uint16_t getNextFileBlock() override {}
    virtual bool isLastBlock() override {}
    virtual bool getNextDirectoryEntry() override;
    virtual bool isInitialized() {
        return true;
    };
    virtual void writeBufferToFile(uint16_t numBytes) override {}
    virtual void updateBlock() override {}
    virtual void closeFile() override {}
    virtual void openCurrentDirectory() override {}
    
    virtual uint8_t* getFilename() override { return nullptr; }
    virtual uint8_t* getBuffer() override { return nullptr; }
    virtual uint16_t writeBufferSize() override {
        return 512;
    };

private:
    uint8_t* _buffer = nullptr;
    uint8_t* _file_buffer = nullptr;
};

}