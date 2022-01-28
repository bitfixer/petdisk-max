#include <stdint.h>
#include <MD5Builder.h>
#include <Update.h>
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

// SD card updater
int sd_card_update(bitfixer::FAT32* fs, Logger* logger)
{
    int written = 0;
    int ret = 0;
    int bytesWritten = 0;
 
    // open firmware file
    MD5Builder fwMd5;

    char fname[256];
    uint8_t* f = fs->getFilename();
    logger->printf("filename: %s\n", f);
    strcpy(fname, (const char*)f);
    fs->openFileForReading((uint8_t*)fname);
    uint32_t fileSize = fs->getFileSize();

    logger->printf("file size: %d\n", fileSize);

    // read entire file and generate md5
    uint16_t b = 512;
    int count = 0;

    fwMd5.begin();
    while (b == 512)
    {
        b = fs->getNextFileBlock();
        if (b > 0)
        {
            fwMd5.add(fs->getBuffer(), b);
            count += b;
        }
    }

    // calculate md5
    fwMd5.calculate();
    logger->printf("firmware md5: %s count %d", fwMd5.toString().c_str(), count);

    // reopen file
    fs->openFileForReading((uint8_t*)fname);
    if (Update.begin(fileSize, U_FLASH)){
        if (!Update.setMD5(fwMd5.toString().c_str()))
        {
            logger->printf("error setting md5\n");
            return -1;
        }

        int lastFlashingPct = -1;
        logger->printf("flashing: ");
        while (!Update.isFinished()) {
            //read sdcard
            uint16_t numBytes = fs->getNextFileBlock();
            written = Update.write(fs->getBuffer(), numBytes);
            if (written > 0) {
                if(written != numBytes){
                    logger->printf("Flashing chunk not full ... warning!\n");
                }
                bytesWritten += written;

                int pct = (100*bytesWritten)/fileSize;
                if (pct > lastFlashingPct && pct % 10 == 0)
                {
                    logger->printf("%d ", (100*bytesWritten)/fileSize);
                }
                lastFlashingPct = pct;
            } else {
                logger->printf("Flashing ... failed!\n");
                ret = -1;
                break;
            }
        }
        
        logger->printf("\ndone writing to flash\n");
        if(bytesWritten == fileSize && Update.end()){
            logger->printf("Flashing ... done!\n");
            ret = 1;              
        } else {
            logger->printf("Flashing or md5 ... failed!"); 
            ret = -1;
        }
    } else {
        logger->printf("Flashing init ... failed!");
        ret = -1;
    } 
        
    return ret;
}

void firmware_detected_action(bitfixer::FAT32* fs, Logger* logger)
{
    // firmware detected, attempt to perform update
    int ret = sd_card_update(fs, logger);
    if (ret == 1)
    {
        logger->printf("SD update complete.\n");
        // reset into the new firmware
        ESP.restart();
    }
    else
    {
        logger->printf("SD update failed.\n");
    }
}

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