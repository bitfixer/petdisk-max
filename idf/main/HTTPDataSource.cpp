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

    if (length > 0) {
        for (int i = 0; i < 32; i++) {
            ESP_LOGI(TAG, "%d: %X", i, _file_buffer[i]);
        }
    }

    return (bool)(length > 0);
}


}