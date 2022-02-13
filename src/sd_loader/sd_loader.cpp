#include <stdint.h>
#include <MD5Builder.h>
#include <Update.h>
#include "esp_ota_ops.h"
#include "../SerialLogger.h"
#include "../SPI_routines.h"
#include "../SD_routines.h"
#include "../hardware.h"

uint8_t _buffer[1024];
char _expectedMd5[33];
char _firmwareFilename[13];

bitfixer::Serial1 _logSerial;
bitfixer::SerialLogger _logger;
bSPI _spi;
SD _sd;
bitfixer::FAT32 _fat32;

void blink_led(int count, int ms_on, int ms_off)
{
    set_led(false);
    for (int i = 0; i < count; i++)
    {
        set_led(true);
        hDelayMs(ms_on);
        set_led(false);
        hDelayMs(ms_off);
    }
}

// SD card updater
int sd_card_update(bitfixer::FAT32* fs, Logger* logger)
{
    int written = 0;
    int ret = 0;
    int bytesWritten = 0;
 
    // open firmware file
    MD5Builder fwMd5;

    fs->openFileForReading((uint8_t*)_firmwareFilename);
    uint32_t fileSize = fs->getFileSize();

    logger->printf("file size: %d\n", fileSize);
    // blink led quickly during update
    bool led_state = false;
    set_led(false);

    // read entire file and generate md5
    uint16_t b = 512;
    int count = 0;
    int blocksPerBlink = 10;

    fwMd5.begin();
    int numBlocks = 0;
    while (b == 512)
    {
        b = fs->getNextFileBlock();
        if (b > 0)
        {
            numBlocks++;
            if (numBlocks % blocksPerBlink == 0)
            {
                led_state = !led_state;
                set_led(led_state);
            }
            fwMd5.add(fs->getBuffer(), b);
            count += b;
        }
    }

    // calculate md5
    fwMd5.calculate();
    logger->printf("firmware md5: %s count %d\n", fwMd5.toString().c_str(), count);

    // check for match with md5 from file
    if (strlen(_expectedMd5) > 0) {
        if (strcmp(fwMd5.toString().c_str(), _expectedMd5) != 0)
        {
            logger->printf("calculated md5 does not match, cancelling update\n");
            return -1;
        }
        else
        {
            logger->printf("md5 matches.\n");
        }
    }

    set_led(false);

    // reopen file
    fs->openFileForReading((uint8_t*)_firmwareFilename);
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
                if (pct > 0 && pct > lastFlashingPct && pct % 10 == 0)
                {
                    // blink once every 10% progress
                    blink_led(1, 150, 150);
                    logger->printf("%d ", pct);
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

// reboot into other boot partition.
void no_firmware_action(Logger* logger)
{
    const esp_partition_t* partition = esp_ota_get_next_update_partition(NULL);
    esp_err_t err = esp_ota_set_boot_partition(partition);
    logger->printf("update boot partition, result %d\n", err);
    ESP.restart();    
}

bool checkForFirmware(char* buffer, bitfixer::FAT32* fat32, bitfixer::SerialLogger* log)
{
    char md5Filename[13];

    log->printf("checking\r\n");
    if (!fat32->init())
    {
        log->printf("noinit\r\n");
        return false;
    }

    // check for existence of firmware file
    fat32->openCurrentDirectory();
    
    // try to find a firmware file
    bool found = false;
    while (fat32->getNextDirectoryEntry())
    {
        uint8_t* fname = fat32->getFilename();
        if (strlen((char*)fname) != 12)
        {
            continue;
        }

        if (
            fname[0] == 'F' && 
            fname[1] == 'I' && 
            fname[2] == 'R' && 
            fname[3] == 'M' &&
            fname[9] == 'B' &&
            fname[10] == 'I' &&
            fname[11] == 'N') 
        {
            strcpy(_firmwareFilename, (char*)fname);
            found = true;
            break;
        }
    }

    if (!found)
    {
        log->printf("no firmware\r\n");
        return false;
    }
    
    // get filename
    log->printf("found firmware, filename: %s\n", _firmwareFilename);

    // check for md5 file with the same prefix
    strcpy(md5Filename, _firmwareFilename);
    md5Filename[9] = 'M';
    md5Filename[10] = 'D';
    md5Filename[11] = '5';

    fat32->openCurrentDirectory();
    if (fat32->findFile(md5Filename))
    {
        log->printf("found md5 file: %s\n", buffer);
        // read from this file and store the md5
        if (fat32->openFileForReading((uint8_t*)md5Filename))
        {
            fat32->getNextFileBlock();
            memcpy(_expectedMd5, fat32->getBuffer(), 32);
            log->printf("md5: %s\n", _expectedMd5);
        }
    }
    else
    {
        log->printf("md5 file not found: %s\n", md5Filename);
    }

    // now find firmware file
    fat32->openCurrentDirectory();
    if (!fat32->findFile(_firmwareFilename))
    {
        log->printf("no firmware\r\n");
        return false;
    }

    log->printf("got firmware: %s\n", _firmwareFilename);

    return true;
}

void setup()
{
    // clear expected md5
    memset(_expectedMd5, 0, 33);
    memset(_firmwareFilename, 0, 13);
    
    // initialize led
    init_led();
    set_led(false);

    _logSerial.init(115200);
    _logger.initWithSerial(&_logSerial);
    _logger.printf("checking for firmware\n");

    _spi.init();
    _sd.initWithSPI(&_spi, spi_cs());
    _fat32.initWithParams(&_sd, _buffer, &_buffer[512], &_logger);

    bool hasFirmware = checkForFirmware((char*)&_buffer[769], &_fat32, &_logger);
    if (hasFirmware)
    {
        _logger.printf("has firmware: %s\n", _fat32.getFilename());
        blink_led(2, 150, 150);
        firmware_detected_action(&_fat32, &_logger);
    }
    else
    {
        _logger.printf("no firmware\n");
        blink_led(3, 150, 150);
        no_firmware_action(&_logger);
    }
}

void loop()
{
    _logger.printf("loop\n");
    hDelayMs(1000);
}