#pragma once

#include "DataSource.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <esp_heap_caps.h>

namespace bitfixer {

class HTTPDataSource : public DataSource {
public:
    HTTPDataSource() {
        _buffer = (uint8_t*)heap_caps_malloc(512, MALLOC_CAP_SPIRAM);
        _file_buffer = (uint8_t*)heap_caps_malloc(64*1024, MALLOC_CAP_SPIRAM);
    }
    ~HTTPDataSource() {
        if (_buffer) {
            heap_caps_free(_buffer);
        }

        if (_file_buffer) {
            heap_caps_free(_file_buffer);
        }
    }

    virtual bool init() override { return true; }
    virtual void openFileForWriting(uint8_t* fileName) override {}
    virtual bool openFileForReading(uint8_t* fileName) override;
    virtual bool openDirectory(const char* dirName) override { return true; }
    virtual uint16_t getNextFileBlock() override { return 0; }
    virtual bool isLastBlock() override { return false; }
    virtual bool getNextDirectoryEntry() override { return true; }
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