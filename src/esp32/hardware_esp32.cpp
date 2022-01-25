#include "hardware.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <SPI.h>
#include <MD5Builder.h>
#include <Update.h>

#define LED_PIN     2
#define CS_PIN      4

uint8_t spi_cs()
{
    return CS_PIN;
}

void prog_init()
{
    // nothing for esp32
    set_atn_input();
    EEPROM.begin(512);    
}
 
void reset_esp()
{
    // nothing for esp32
}

void init_led()
{
    // nothing for esp32
    pinMode(LED_PIN, OUTPUT);
}

void set_led(bool value)
{
    if (value == true)
    {
        digitalWrite(2, HIGH);
    }
    else
    {
        digitalWrite(2, LOW);
    }
}

void hDelayMs(int ms)
{
    delay(ms);
}

uint8_t bf_pgm_read_byte(uint8_t* src)
{
    return pgm_read_byte(src);
}

void bf_eeprom_write_block(const void* block, void* eeprom, size_t n)
{
    EEPROM.writeBytes((int)eeprom, block, n);
    EEPROM.commit();
}

void bf_eeprom_read_block(void* block, const void* eeprom, size_t n)
{
    EEPROM.readBytes((int)eeprom, (void*)block, n);
}

uint8_t bf_eeprom_read_byte(const uint8_t* addr)
{
    return EEPROM.readByte((int)addr);
}

// SPI

void spi_init()
{
    pinMode(CS_PIN, OUTPUT);
    spi_cs_unselect();
    SPI.begin();
}

uint8_t spi_transmit(uint8_t data)
{
    return SPI.transfer(data);
}

void spi_cs_select()
{
    digitalWrite(CS_PIN, LOW);
}

void spi_cs_unselect()
{
    digitalWrite(CS_PIN, 1);
}

// serial

void serial0_init(uint32_t baudRate)
{
    // N/A
}

void serial0_transmitByte(unsigned char data)
{
   // N/A
}

unsigned char serial0_receiveByte()
{
    return 0;
}

void serial0_enable_interrupt()
{
    // N/A
}

void serial0_disable_interrupt()
{
    // N/A
}

void serial1_init(uint32_t baudRate)
{
    ::Serial.begin(baudRate);
}

void serial1_transmitByte(unsigned char data)
{
    ::Serial.write(data);
}

unsigned char serial1_receiveByte()
{
    int ret = ::Serial.read();
    while (ret == -1)
    {
        ret = ::Serial.read();
    }

    return (unsigned char)ret;
}

void serial1_enable_interrupt()
{
    // N/A
}

void serial1_disable_interrupt()
{
    // N/A
}

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

        // remove firmware file(s)
        //fs->openCurrentDirectory();
        logger->printf("finding firmware file\n");
        char buffer[32];
        sprintf_P((char*)buffer, PSTR("FIRM*"));
        fs->openCurrentDirectory();
        if (!fs->findFile(buffer))
        {
            logger->printf("firmware file not found after update - unexpected\n");
            return;
        }

        logger->printf("deleting firmware file %s\n", (char*)fs->getFilename());
        fs->deleteFile();

        // reset into the new firmware
        ESP.restart();
    }
    else
    {
        logger->printf("SD update failed.\n");
    }
}