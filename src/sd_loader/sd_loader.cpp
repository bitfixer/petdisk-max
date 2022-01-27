#include <stdint.h>
#include "esp_ota_ops.h"
#include "../SerialLogger.h"
#include "../SPI_routines.h"
#include "../SD_routines.h"
#include "../hardware.h"

uint8_t _buffer[1024];

bitfixer::Serial1 _logSerial;
bitfixer::SerialLogger _logger;
bSPI _spi;
SD _sd;
bitfixer::FAT32 _fat32;

void no_firmware_action(Logger* logger)
{
    const esp_partition_t* partition = esp_ota_get_next_update_partition(NULL);
    esp_err_t err = esp_ota_set_boot_partition(partition);
    logger->printf("update boot partition, result %d\n", err);
    ESP.restart();    
}

bool checkForFirmware(char* buffer, bitfixer::FAT32* fat32, bitfixer::Serial1* log)
{
    log->transmitStringF(PSTR("checking\r\n"));
    if (!fat32->init())
    {
        log->transmitStringF(PSTR("noinit\r\n"));
        return false;
    }

    sprintf_P(buffer, PSTR("FIRM*"));
    fat32->openCurrentDirectory();
    if (!fat32->findFile(buffer))
    {
        log->transmitStringF(PSTR("nofirm\r\n"));
        return false;
    }

    log->transmitStringF(PSTR("gotfirmware\r\n"));

    return true;
}

void setup()
{
    prog_init();
    _logSerial.init(115200);
    _logger.initWithSerial(&_logSerial);
    _logger.printf("done setup\n");

    _spi.init();
    _sd.initWithSPI(&_spi, spi_cs());
    _fat32.initWithParams(&_sd, _buffer, &_buffer[512], &_logger);

    bool hasFirmware = checkForFirmware((char*)&_buffer[769], &_fat32, &_logSerial);
    if (hasFirmware)
    {
        _logger.printf("has firmware: %s\n", _fat32.getFilename());
        firmware_detected_action(&_fat32, &_logger);
    }
    else
    {
        _logger.printf("no firmware\n");
        no_firmware_action(&_logger);
    }
}

void loop()
{
    _logger.printf("loop\n");
    hDelayMs(1000);
}