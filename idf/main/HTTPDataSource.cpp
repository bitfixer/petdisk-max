#include "HTTPDataSource.h"
#include <esp_log.h>
#include "http.h"
#include "console.h"

namespace bitfixer {

static const char* TAG = "HTTPDS";

bool HTTPDataSource::openFileForReading(uint8_t* fileName) {
    ESP_LOGI(TAG, "open file for reading: %s", (char*)fileName);
    
    // fetch entire file
    char str[256];
    sprintf(str, "http://bitfixer.com/pd/petdisk.php?file=%s", fileName);

    int size = 64*1024;
    ESP_LOGI(TAG, "url is: %s, bufsize %d", str, size);

    int length = HTTP::request(str, _file_buffer, size);
    ESP_LOGI(TAG, "result: %d", length);

    _file_pos = -1;
    if (length > 0) {
        _file_size = length;
    } else {
        _file_size = -1;
    }

    return (bool)(length > 0);
}

uint16_t HTTPDataSource::getNextFileBlock() {
    if (_file_pos == -1) {
        _file_pos = 0;
    } else {
        _file_pos += readBufferSize();
    }

    int block_size = readBufferSize();
    int rem = _file_size - _file_pos;
    if (block_size > rem) {
        block_size = rem;
    }

    memcpy(_buffer, &_file_buffer[_file_pos], block_size);
    return (uint16_t)block_size;
}

bool HTTPDataSource::isLastBlock() {
    int next_pos = 0;
    if (_file_pos >= 0) {
        next_pos = _file_pos + readBufferSize();
    }
    if (next_pos >= _file_size) {
        return true;
    }
    return false;
}


}