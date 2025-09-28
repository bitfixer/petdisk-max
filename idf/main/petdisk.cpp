#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <driver/uart.h>
#include "EspConn.h"
#include "EspHttp.h"
#include "IEEE488.h"
#include "DataSource.h"
#include "NetworkDataSource.h"
#include "D64DataSource.h"
#include "helpers.h"
#include "hardware.h"
#include "githash.h"
#include "FAT32.h"
#include "console.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

//#define TESTING 1

static const char* TAG = "PD";

// global memory buffer
uint8_t _buffer[1024];
uint16_t _bufferSize;

// addresses for PET IEEE commands
#define PET_LOAD_FNAME_ADDR     0xF0
#define PET_SAVE_FNAME_ADDR     0xF1
#define PET_OPEN_FNAME_MASK     0xF0
#define PET_READ_CMD_ADDR       0x60
#define PET_SAVE_CMD_ADDR       0x61
#define PET_OPEN_IO_ADDR        0x60
#define PET_CLOSE_FILE          0xE0

#define PET_ADDRESS_MASK        0x0F

#define DIR_BACK_CHARACTER      0x5F

const uint8_t _dirHeader[] PROGMEM =
{
    0x01,
    0x04,
    0x1F,
    0x04,
    0x00,
    0x00,
    0x12
};

const uint8_t _versionString[] PROGMEM = "\"PETDISK MAX V2.0\"      ";
const uint8_t _firmwareString[] PROGMEM = "BUILD ";
const uint8_t _fileExtension[] PROGMEM =
{
    '.',
    'P',
    'R',
    'G',
    0x00,
};

const uint8_t _seqExtension[] PROGMEM =
{
    '.',
    'S',
    'E',
    'Q',
    0x00,
};

void blink_led(int count, int ms_on, int ms_off)
{
    set_led(false);
    for (int i = 0; i < count; i++)
    {
        set_led(true);
        for (int j = 0; j < ms_on; j++)
        {
            hDelayMs(1);
        }
        set_led(false);
        for (int j = 0; j < ms_off; j++)
        {
            hDelayMs(1);
        }
    }
}

bool checkForDisable(char* buffer, bitfixer::FAT32* fat32)
{
    if (!fat32->init())
    {
        return false;
    }

    fat32->openCurrentDirectory();
    sprintf_P(buffer, PSTR("DISABLE.PD"));
    if (fat32->findFile(buffer))
    {
        return true;
    }

    return false;
}

void checkForFirmware(char* buffer, bitfixer::FAT32* fat32)
{
    log_i("checking for firmware");
    if (!fat32->init())
    {
        log_i("cannot init sd card");
        return;
    }

    // first check for NODELETE.FRM, 
    // if this file exists, do not delete any firmware files.
    // Useful during programming
    sprintf_P(buffer, PSTR("NODELETE.FRM"));
    fat32->openCurrentDirectory();
    if (fat32->findFile(buffer))
    {
        log_i("nodelete");
        return;
    }

    // check for existence of firmware file
    // name specific to architecture (atmega, esp32)

    fat32->openCurrentDirectory();
    bool foundFirmware = false;
    while (fat32->getNextDirectoryEntry() == true)
    {
        unsigned char* name = fat32->getFilename();
        upperStringInPlace((char*)name);

        log_i("%s", name);

        if (isFirmwareFile((char*)name))
        {
            log_i("%s is firmware", name);
            foundFirmware = true;
            break;
        }
    }

    if (!foundFirmware)
    {
        log_i("no firmware found");
        return;
    }

    log_i("firmware found, deleting");
    fat32->deleteFile();
    return;
}

void sd_test(DataSource* dataSource)
{
    if (!dataSource->init())
    {
        log_i("no init");
    }

    log_i("init");
}

#define MIN_DEVICE_ID 8
#define MAX_DEVICE_ID 16

// define a configuration structure
// this will be written to eeprom

#define DEVICE_NONE         0  
#define DEVICE_SD0          1
#define DEVICE_SD1          2
#define DEVICE_URL_BASE     3
#define DEVICE_END          3

#define FILENAME_MAX_LENGTH 21

struct PACKED pd_config {
    uint8_t device_type[9];
    char urls[4][64];
    char wifi_ssid[33];
    char wifi_password[64];
};

typedef enum _pdstate
{
    IDLE,
    BUS_LISTEN,
    BUS_TALK,
    LOAD_FNAME_READ,
    SAVE_FNAME_READ,
    OPEN_FNAME_READ,
    OPEN_FNAME_READ_DONE,
    OPEN_FNAME_READ_DONE_FOR_WRITING,
    OPEN_DATA_WRITE,
    OPEN_DATA_WRITE_DONE,
    OPEN_DATA_READ,
    DIR_READ,
    FILE_READ_OPENING,
    FILE_SAVE_OPENING,
    FILE_READ,
    FILE_SAVE,
    OPEN_FNAME,
    FILE_NOT_FOUND,
    CLOSING
} pdstate;

typedef enum _filedir
{
    FNONE,
    FREAD,
    FWRITE
} filedir;

typedef struct _openFileInfo
{
    char _fileName[FILENAME_MAX_LENGTH];
    filedir _fileDirection;
    int _fileBufferIndex;
    bool _useRemainderByte;
    int _byteIndex;
    uint8_t _remainderByte;
    uint8_t _nextByte;
    bool _opened;
    bool _command;
} openFileInfo;

class PETdisk
{
public:
    PETdisk()
    : _espConn(0)
    , _espHttp(0)
    {
        for (int i = 0; i < 8; i++)
        {
            _dataSources[i] = 0;
        }

        _timeDataSource = NULL;
    }
    ~PETdisk() {}

    void init(
        bitfixer::FAT32* fat32,
        D64DataSource* d64,
        uint8_t* buffer, 
        uint16_t* bufferSize, 
        bitfixer::EspConn* espConn, 
        bitfixer::EspHttp* espHttp, 
        bitfixer::NetworkDataSource** nds_array,
        bitfixer::IEEE488* ieee);
    void setDataSource(uint8_t id, DataSource* dataSource);
    DataSource* getDataSource(uint8_t id);

    bool readConfigFile(bitfixer::FAT32* fat32, uint8_t* buffer);
    void printConfig(struct pd_config* pdcfg);
    void loop();

    bitfixer::NetworkDataSource* _timeDataSource;

private:
    DataSource* _dataSources[8];
    bitfixer::EspConn* _espConn;
    bitfixer::EspHttp* _espHttp;
    bitfixer::IEEE488* _ieee;
    bitfixer::FAT32* _fat32;
    D64DataSource* _d64;

    openFileInfo _openFileInformation[14];

    DataSource* _dataSource;
    uint8_t* progname;
    int _filenamePosition;
    int _fileNotFound;
    int _bytesToSend;
    pdstate _currentState;
    int _bufferFileIndex;
    uint8_t _primaryAddress;
    uint8_t _secondaryAddress;

    bool _directoryFinished;
    bool _lastDirectoryBlock;
    uint8_t _directoryEntryIndex;
    int8_t _directoryEntryByteIndex;
    uint8_t _directoryEntry[32];
    uint8_t _directoryNextByte;
    uint16_t _directoryEntryAddress;

    bool configChanged(struct pd_config* pdcfg);
    uint8_t processFilename(uint8_t* filename, uint8_t length, bool* write);
    bool processCommand(uint8_t* command);
    bool writeFile();
    uint8_t get_device_address();
    openFileInfo* getFileInfoForAddress(uint8_t address);
    void resetFileInformation(uint8_t address);
    bool isD64(const char* fileName);
    bool openFile(uint8_t* fileName);

    void initDirectory();
    bool getDirectoryEntry();
};

bool petdisk_config_valid(struct pd_config* cfg) {
    // first check if any device has been set
    // at least one device should be set to a value other than DEVICE_NONE
    bool device_set = false;
    for (int i = 0; i < 9; i++) {
        if (cfg->device_type[i] != DEVICE_NONE) {
            device_set = true;
            break;
        }
    }

    if (!device_set) {
        log_i("no devices set, not valid config");
        return false;
    }

    // check if any devices have an invalid type
    for (int i = 0; i < 9; i++) {
        if (cfg->device_type[i] > DEVICE_END) {
            log_i("device %d->%d, invalid", i, cfg->device_type[i]);
            return false;
        }
    }

    return true;
}

void PETdisk::init(
    bitfixer::FAT32* fat32,
    D64DataSource* d64,
    uint8_t* buffer, 
    uint16_t* bufferSize, 
    bitfixer::EspConn* espConn, 
    bitfixer::EspHttp* espHttp, 
    bitfixer::NetworkDataSource** nds_array,
    bitfixer::IEEE488* ieee)
{
    _fat32 = fat32;

    // default time and date for FAT32
    _fat32->setDateTime(2022, 01, 01, 0, 0, 0);

    _d64 = d64;
    _ieee = ieee;

    // reset state variables
    _dataSource = 0;
    progname = (uint8_t*)&_buffer[1024-FILENAME_MAX_LENGTH];
    _filenamePosition = 0;
    _fileNotFound = 0;
    _bytesToSend = 0;
    _currentState = IDLE;
    _bufferFileIndex = -1;
    _secondaryAddress = 0;

    // clear all open file info structs
    for (int i = 8; i < 16; i++)
    {
        resetFileInformation(i);
    }

    char tmp[32];
    if (readConfigFile(fat32, buffer))
    {
        log_i("read config file");
    }
    else
    {
        log_i("no config read");
    }

    struct pd_config *pdcfg = (struct pd_config*)&buffer[512];
    
    bf_eeprom_read_block(pdcfg, (void*)0, sizeof(struct pd_config));
    log_i("----");
    printConfig(pdcfg);
    log_i("----");

    _espConn = espConn;
    _espHttp = espHttp;
    bool espConnected = false;
    
    // check validity of config
    //if (pdcfg->device_type[0] > DEVICE_END)
    if (!petdisk_config_valid(pdcfg))
    {
        // config not set
        // use defaults
        setDataSource(8, fat32);
        log_i("using default");
        return;
    }

    if (strlen(pdcfg->wifi_ssid) > 0 && strlen(pdcfg->wifi_password) > 0)
    {
        log_i("attempting wifi connection");
        if (_espConn->connect(pdcfg->wifi_ssid, pdcfg->wifi_password))
        {
            log_i("wifi connect");
            log_i("ssid: %s", pdcfg->wifi_ssid);
            log_i("password: %s", pdcfg->wifi_password);
            espConnected = true;
            blink_led(2, 150, 150);
        }
        else
        {
            log_i("no connect");
            blink_led(5, 150, 150);
        }
    }

    // check each entry in config file
    int network_drive_count = 0;
    for (int i = 0; i < 9; i++)
    {
        sprintf_P(tmp, PSTR("i %d dt %d"), i, pdcfg->device_type[i]);
        //log_i(tmp);
        if (pdcfg->device_type[i] == DEVICE_SD0)
        {
            setDataSource(i + MIN_DEVICE_ID, fat32);
            log_i("SD0");
        }
        else if (pdcfg->device_type[i] >= DEVICE_URL_BASE && network_drive_count < 4)
        {
            int device_id = i + MIN_DEVICE_ID;
            int url_index = pdcfg->device_type[i] - DEVICE_URL_BASE;
            int eeprom_offset = 9 + (64 * url_index);

            // find the host and url portion for this datasource
            char* url = strchr(pdcfg->urls[url_index], '/');
            int url_offset = url - pdcfg->urls[url_index];

            // check for specified port in host string
            int hostEndIndex = url_offset;
            char* portSeparator = strchr(pdcfg->urls[url_index], ':');
            int port = 80;
            if (portSeparator != NULL && portSeparator < url) 
            {
                sscanf(portSeparator, ":%d/", &port);
                log_i("port is %d\n", port);
                hostEndIndex = portSeparator - pdcfg->urls[url_index];
            }

            bitfixer::NetworkDataSource* nds = nds_array[network_drive_count];

            log_i("setting port: %d\n", port);
            nds->setUrlData(
                (void*)eeprom_offset, 
                hostEndIndex,
                port,
                (void*)(eeprom_offset+url_offset), 
                strlen(pdcfg->urls[url_index]) - url_offset);

            sprintf_P(tmp, PSTR("d %d %d %d\r\n"), device_id, eeprom_offset, url_offset);

            if (espConnected)
            {
                if (nds == NULL) {
                    log_i("setting null ds to %d", device_id);
                } else {
                    log_i("setting ds to %d", device_id);
                }
                setDataSource(device_id, nds);
                nds->init();

                if (_timeDataSource == NULL)
                {
                    _timeDataSource = nds;
                }
            }
            else
            {
                setDataSource(device_id, 0);
            }

            network_drive_count++;
        }
    }

    _ieee->unlisten();
}

void PETdisk::printConfig(struct pd_config* pdcfg)
{
    for (int i = 0; i < 9; i++)
    {
        log_i("d %d->%d", i, pdcfg->device_type[i]);
    }

    for (int i = 0; i < 4; i++)
    {
        log_i("%d: %s", i, pdcfg->urls[i]);
    }

    log_i("ssid: %s", pdcfg->wifi_ssid);
    log_i("pass: %s", pdcfg->wifi_password);
}

bool PETdisk::configChanged(struct pd_config* pdcfg)
{
    // check pdconfig against version in eeprom
    uint8_t byte;
    uint8_t* cfg = (uint8_t*)pdcfg;
    for (int i = 0; i < (int)sizeof(struct pd_config); i++)
    {
        byte = bf_eeprom_read_byte((const uint8_t*)i);
        if (byte != cfg[i])
        {
            return true;
        }
    }
    return false;
}

static bool parseCSVLine(const char* line, char* key, char* value) {
    char* separator = strchr(line, ',');
    if (!separator) {
        return false;
    }

    int keylen = separator-line;
    memcpy(key, line, keylen);
    key[keylen] = 0;

    separator++;
    if (*separator == '"') {
        separator++;
    }

    char* lineend = separator;
    // skip until end of line
    while (*lineend != 0 && *lineend != '\r' && *lineend != '\n') {
        lineend++;
    }

    int valuelen = lineend-separator;
    memcpy(value, separator, valuelen);
    if (value[valuelen-1] == '"') {
        valuelen--;
    }
    value[valuelen] = 0;
    return true;
}

bool PETdisk::readConfigFile(bitfixer::FAT32* fat32, uint8_t* buffer)
{
    // check for config file
    char cfg_fname[32];
    const char* config_filename = PSTR("PETDISK.CFG");
    if (!fat32->init())
    {
        return false;
    }

    sprintf_P(cfg_fname, config_filename);
    fat32->openCurrentDirectory();
    if (!fat32->openFileForReading((uint8_t*)cfg_fname))
    {
        return false;
    }

    // read config file
    fat32->getNextFileBlock();

    char* token;
    char* next;
    char* str[2];

    struct pd_config* pdcfg = (struct pd_config*)&buffer[512];

    str[0] = (char*)&buffer[512+sizeof(struct pd_config)];
    str[1] = (char*)&buffer[512+sizeof(struct pd_config)+25];

    
    int id;
    int url_index = 0;
    memset(pdcfg, 0, sizeof(struct pd_config));

    token = (char*)fat32->getBuffer();
    next = strchr((char*)fat32->getBuffer(), '\n');
    while (token != 0)
    {
        log_i("token %s", token);
        if (!parseCSVLine(token, str[0], str[1])) {
            break;
        }

        id = atoi(str[0]);
        if (id == 0)
        {
            if (str[0][0] == 's' || str[0][0] == 'S')
            {
                // use next field as ssid
                log_i("ssid: %s", str[1]);
                strcpy(pdcfg->wifi_ssid, str[1]);
            }
            else if (str[0][0] == 'p' || str[0][0] == 'P')
            {
                strcpy(pdcfg->wifi_password, str[1]);
                log_i("password: %s", str[1]);
            }
        }
        else
        {
            if (id < MIN_DEVICE_ID || id > MAX_DEVICE_ID)
            {
                log_i("invalid field");
                return false;
            }

            // check if this is an sd card or a url string
            if (str[1][0] == 'S' && str[1][1] == 'D')
            {
                log_i("is sd %d", id);
                if (str[1][2] == '0')
                {
                    pdcfg->device_type[id - MIN_DEVICE_ID] = DEVICE_SD0;
                }
                else if (str[1][2] == '1')
                {
                    pdcfg->device_type[id - MIN_DEVICE_ID] = DEVICE_SD1;
                }
                else
                {
                    log_i("invalid sd");
                }
            }
            else
            {
                // this is a url
                pdcfg->device_type[id - MIN_DEVICE_ID] = DEVICE_URL_BASE + url_index;
                // copy the url string
                strcpy(pdcfg->urls[url_index], str[1]);
                url_index++;
            }
        }

        token = next;
        if (token != 0)
        {
            token++;
            next = strchr(token, '\n');
        }
    }

    // check for change in configuration
    if (configChanged(pdcfg))
    {
        // update eeprom with new config
        log_i("config updated");
        bf_eeprom_write_block(pdcfg, (void*)0, sizeof(struct pd_config));
        blink_led(3, 150, 150);
    }
    else
    {
        log_i("config not updated");
        blink_led(4, 150, 150);
    }

    return true;
}

void PETdisk::setDataSource(uint8_t id, DataSource* dataSource)
{
    if (id >= MIN_DEVICE_ID && id <= MAX_DEVICE_ID)
    {
        log_d("set ds %d %d", id, id-MIN_DEVICE_ID);
        if (dataSource == NULL) {
            log_d("setting null ds");
        }
        _dataSources[id-MIN_DEVICE_ID] = dataSource;
    }
}

DataSource* PETdisk::getDataSource(uint8_t id)
{
    log_d("getDataSource %d", id);
    if (id < MIN_DEVICE_ID || id > MAX_DEVICE_ID)
    {
        log_d("out of range");
        return 0;
    }

    DataSource* ds = _dataSources[id-MIN_DEVICE_ID];
    if (ds == NULL) {
        log_d("null ds");
    }

    return _dataSources[id-MIN_DEVICE_ID];
}

bool PETdisk::writeFile()
{
    uint16_t numBytes;
    uint8_t rdchar;
    uint16_t writeBufferSize = _dataSource->writeBufferSize();
    
    numBytes = 0;
    do
    {
        if (!_ieee->get_byte_from_bus(rdchar)) {
            return false;
        }
        _dataSource->getBuffer()[numBytes++] = rdchar;
        
        if (numBytes >= writeBufferSize)
        {
            _dataSource->writeBufferToFile(numBytes);
            numBytes = 0;
        }
        
        if (!_ieee->acknowledge_bus_byte()) {
            return false;
        }
        _ieee->signal_ready_for_data();
    }
    while (!_ieee->eoi_is_low());
    
    if (numBytes > 0)
    {
        _dataSource->writeBufferToFile(numBytes);
    }
    
    if (_timeDataSource != NULL && _dataSource->needRealTime())
    {
        // attempt to fetch current date and time
        if (_timeDataSource != NULL)
        {
            int year, month, day, hour, minute, second;
            bool gotTimeDate = _timeDataSource->getCurrentDateTime(&year, &month, &day, &hour, &minute, &second);
            if (gotTimeDate)
            {
                // set time and date on original datasource
                _dataSource->setDateTime(year, month, day, hour, minute, second);
            }
        }
    }

    _dataSource->closeFile();
    return true;
}

void PETdisk::initDirectory()
{
    _directoryEntryIndex = 0;
    _directoryEntryAddress = 0x041f;
    _directoryEntryByteIndex = 0;

    // copy the directory header
    memcpy((uint8_t *)_directoryEntry, (uint8_t *)_dirHeader, 7);

    // print directory title
    memcpy((uint8_t *)&_directoryEntry[7], (uint8_t *)_versionString, 24);
    _directoryEntry[31] = 0x00;
    _directoryFinished = false;
    _lastDirectoryBlock = false;
    _directoryNextByte = _directoryEntry[0];
}

bool PETdisk::getDirectoryEntry()
{
    bool gotDir;
    uint8_t startline;
    
    if (_directoryFinished)
    {
        startline = 0;
        _directoryEntryAddress += 0x001e;
        _directoryEntry[startline] = (uint8_t)(_directoryEntryAddress & 0x00ff);
        _directoryEntry[startline+1] = (uint8_t)((_directoryEntryAddress & 0xff00) >> 8);
        _directoryEntry[startline+2] = 0xff;
        _directoryEntry[startline+3] = 0xff;
        sprintf_P((char *)&_directoryEntry[startline+4], PSTR("BLOCKS FREE.             "));
        _directoryEntry[startline+29] = 0x00;
        _directoryEntry[startline+30] = 0x00;
        _directoryEntry[startline+31] = 0x00;
        _lastDirectoryBlock = true;
        return true;
    }

    do
    {
        // get next directory entry
        gotDir = _dataSource->getNextDirectoryEntry();
        if (gotDir == false)
        {
            _directoryEntryAddress += 0x0020;
            startline = 0;
            memset(_directoryEntry, ' ', 32);
            _directoryEntry[startline] = (uint8_t)(_directoryEntryAddress & 0x00ff);
            _directoryEntry[startline+1] = (uint8_t)((_directoryEntryAddress & 0xff00) >> 8);
            _directoryEntry[startline+2] = _directoryEntryIndex+1;
            _directoryEntry[startline+3] = 0x00;
            _directoryEntry[startline+4] = 0x20;
            _directoryEntry[startline+5] = 0x20;
            memcpy(&_directoryEntry[startline+6], (uint8_t*)_firmwareString, 6);
            
            // TODO: add git hash back
            memcpy(&_directoryEntry[startline+6+6], (uint8_t*)_hash, 7);
            _directoryEntry[startline+31] = 0x00;
            _directoryEntryIndex++;
            _directoryFinished = true;
            return false;
        }
        else
        {
            if (!_dataSource->isHidden() && !_dataSource->isVolumeId())
            {
                // check if this is a file that can be used by petdisk
                // currently .PRG and .SEQ, soon .REL

                int fname_length = 0;
                uint8_t* fileName = _dataSource->getFilename();
                if (fileName == NULL)
                {
                    continue;
                }
                fname_length = strlen((char*)fileName);
                bool valid_file = false;
                bool isPrg = false;
                // check for correct extension
                if (_dataSource->isDirectory())
                {
                    valid_file = true;
                }
                // default files with no extension to PRG
                else if (!strstr((const char*)fileName, "."))
                {
                    // no period in filename so no extension
                    // default to PRG type
                    isPrg = true;
                    valid_file = true;
                }
                else if (fname_length > 4)
                {
                    if (toupper(fileName[fname_length-3]) == 'P' &&
                        toupper(fileName[fname_length-2]) == 'R' &&
                        toupper(fileName[fname_length-1]) == 'G')
                    {
                        isPrg = true;
                        valid_file = true;
                    }
                    else if (toupper(fileName[fname_length-3]) == 'S' &&
                        toupper(fileName[fname_length-2]) == 'E' &&
                        toupper(fileName[fname_length-1]) == 'Q')
                    {
                        valid_file = true;
                    }
                    else if (toupper(fileName[fname_length-3]) == 'D' &&
                        toupper(fileName[fname_length-2]) == '6' &&
                        toupper(fileName[fname_length-1]) == '4')
                    {
                        valid_file = true;
                    }
                }

                if (!valid_file)
                {
                    // skip this file, continue to next
                    continue;
                }

                _directoryEntryAddress += 0x0020;
                startline = 0;

                _directoryEntry[startline] = (uint8_t)(_directoryEntryAddress & 0x00ff);
                _directoryEntry[startline+1] = (uint8_t)((_directoryEntryAddress & 0xff00) >> 8);
                _directoryEntry[startline+2] = _directoryEntryIndex+1;
                _directoryEntry[startline+3] = 0x00;
                _directoryEntry[startline+4] = 0x20;
                _directoryEntry[startline+5] = 0x20;
                _directoryEntry[startline+6] = 0x22;

                int extensionPos = -1;

                if (fname_length > 5)
                {
                    if (fileName[fname_length-4] == '.')
                    {
                        extensionPos = fname_length-3;
                        fname_length = fname_length-4;
                    }
                }
                
                if (fname_length >= 17)
                {
                    fname_length = 17;
                }
                
                for (int f = 0; f < fname_length; f++)
                {
                    // make sure filename is upper case
                    _directoryEntry[startline+7+f] = toupper(fileName[f]);
                }

                _directoryEntry[startline+7+fname_length] = 0x22;
                for (int f = 0; f < (17 - fname_length); f++)
                {
                    _directoryEntry[startline+7+fname_length+f+1] = ' ';
                }

                if (_dataSource->isDirectory())
                {
                    _directoryEntry[startline+25] = 'D';
                    _directoryEntry[startline+26] = 'I';
                    _directoryEntry[startline+27] = 'R';
                }
                else
                {
                    if (extensionPos > 0)
                    {
                        // make sure extension is upper case
                        _directoryEntry[startline+25] = toupper(fileName[extensionPos]);
                        _directoryEntry[startline+26] = toupper(fileName[extensionPos+1]);
                        _directoryEntry[startline+27] = toupper(fileName[extensionPos+2]);
                    }
                    else if (isPrg)
                    {
                        _directoryEntry[startline+25] = 'P';
                        _directoryEntry[startline+26] = 'R';
                        _directoryEntry[startline+27] = 'G';
                    }
                    else
                    {
                        _directoryEntry[startline+25] = ' ';
                        _directoryEntry[startline+26] = ' ';
                        _directoryEntry[startline+27] = ' ';
                    }
                }

                _directoryEntry[startline+28] = ' ';
                _directoryEntry[startline+29] = ' ';
                _directoryEntry[startline+30] = ' ';
                _directoryEntry[startline+31] = 0x00;
                _directoryEntryIndex++;

                return false;
            }
        }
    }
    while (gotDir == true);
    return true;
}

uint8_t PETdisk::get_device_address()
{
    uint8_t ieee_address;
    uint8_t buscmd;
    _dataSource = 0;
   
    bool success = false;
    ieee_address = _ieee->get_device_address(&buscmd, &success);
    if (!success)
    {
        return 0;
    }

    _dataSource = getDataSource(ieee_address);
    if (_dataSource == 0)
    {
        _ieee->reject_address();
        return 0;
    }

    _ieee->accept_address();
    _primaryAddress = ieee_address;

    return buscmd;
}

openFileInfo* PETdisk::getFileInfoForAddress(uint8_t address)
{
    if (address >= 2 && address < 16)
    {
        return &_openFileInformation[address - 2];
    }
    else
    {
        return NULL;
    }
}

void PETdisk::resetFileInformation(uint8_t address)
{
    openFileInfo* fileInfo = getFileInfoForAddress(address);
    
    memset(fileInfo->_fileName, 0, FILENAME_MAX_LENGTH);
    fileInfo->_fileDirection = FNONE;
    fileInfo->_fileBufferIndex = 0;
    fileInfo->_useRemainderByte = false;
    fileInfo->_remainderByte = 0;
    fileInfo->_byteIndex = 0;
    fileInfo->_nextByte = 0;
    if (address == 15)
    {
        fileInfo->_opened = true;
        fileInfo->_command = true;
    }
    else
    {
        fileInfo->_opened = false;
        fileInfo->_command = false;
    }
}

bool PETdisk::isD64(const char* fileName)
{
    int len = strlen(fileName);
    
    if (len <= 4)
    {
        return false;
    }

    if (strcmp(".D64", &fileName[len - 4]) == 0)
    {
        return true;
    }

    return false;
}

bool PETdisk::openFile(uint8_t* fileName)
{
    //ESP_LOGI(TAG, "file %s", (char*)fileName);
    if (_dataSource->openFileForReading(fileName))
    {
        return true;
    }

    // if attempting to read PRG file, also check without extension
    char* extPos = strstr((const char*)fileName, ".PRG");
    if (!extPos)
    {
        // already no extension
        return false;
    }

    // remove extension
    *extPos = 0;
    return _dataSource->openFileForReading(fileName);
}

void PETdisk::loop()
{
    // start main loop
    if (_currentState == FILE_NOT_FOUND || _currentState == CLOSING)
    {
        _ieee->unlisten();
        _currentState = IDLE;
        _fileNotFound = 0;
        _filenamePosition = 0;
        memset(progname, 0, FILENAME_MAX_LENGTH);
    }

    if (_ieee->is_unlistened())
    {
        // if we are in an unlisten state,
        // wait for my address     
        uint8_t buscmd = get_device_address();
        if (_dataSource == 0) // no datasource found
        {
            _ieee->unlisten();
            return;
        }

        if (buscmd == LISTEN)
        {
            _currentState = BUS_LISTEN;
        }
        else
        {
            _currentState = BUS_TALK;
        }
    }

    uint8_t rdchar;
    if (!_ieee->get_byte_from_bus(rdchar)) {
        return;
    }

    if (_ieee->atn_is_low()) // check for bus command
    {
        if (rdchar == PET_LOAD_FNAME_ADDR)
        {
            _currentState = LOAD_FNAME_READ;
            _secondaryAddress = 0;
        }
        else if (rdchar == PET_SAVE_FNAME_ADDR)
        {
            _currentState = SAVE_FNAME_READ;
            _secondaryAddress = 0;
        }
        else if ((rdchar & PET_OPEN_FNAME_MASK) == PET_OPEN_FNAME_MASK) // open command to another address
        {
            _currentState = OPEN_FNAME_READ;
            _secondaryAddress = rdchar & PET_ADDRESS_MASK;

            openFileInfo* of = getFileInfoForAddress(_secondaryAddress);
            if (of != NULL)
            {
                of->_fileBufferIndex = -1;
                of->_opened = true;
            }
        }
        else if (rdchar == PET_READ_CMD_ADDR) // read command
        {
            if (_fileNotFound == 1)
            {
                _currentState = FILE_NOT_FOUND;
            }
            else
            {
                _currentState = FILE_READ;
            }
        }
        else if (rdchar == PET_SAVE_CMD_ADDR) // save command
        {
            _currentState = FILE_SAVE;
        }
        else if ((rdchar & PET_OPEN_FNAME_MASK) == PET_OPEN_IO_ADDR) // print or input command
        {
            _secondaryAddress = rdchar & PET_ADDRESS_MASK;

            openFileInfo* of = getFileInfoForAddress(_secondaryAddress);
            if (of != NULL && of->_opened == true)
            {
                if (_currentState == BUS_LISTEN)
                {
                    if (!of->_command && of->_fileBufferIndex == -1)
                    {
                        _dataSource->openFileForWriting((uint8_t*)of->_fileName);
                        of->_fileBufferIndex = 0;
                    }
                    of->_fileDirection = FWRITE;
                    _currentState = OPEN_DATA_WRITE;
                }
                else
                {
                    // file read
                    if (_fileNotFound == 1)
                    {
                        resetFileInformation(_secondaryAddress);
                        _currentState = FILE_NOT_FOUND;
                    }
                    else
                    {
                        of->_fileDirection = FREAD;
                        _currentState = OPEN_DATA_READ;
                    }
                }
            }
            else
            {
                log_i("X %d", _secondaryAddress);
            }
        }
        else if ((rdchar & PET_OPEN_FNAME_MASK) == PET_CLOSE_FILE)
        {
            uint8_t address = rdchar & PET_ADDRESS_MASK;
            openFileInfo* of = getFileInfoForAddress(address);
            if (of != NULL)
            {
                if (of->_fileDirection == FWRITE)
                {
                    if (of->_fileBufferIndex > 0)
                    {
                        _dataSource->writeBufferToFile(of->_fileBufferIndex);
                        of->_fileBufferIndex = 0;
                    }

                    _dataSource->closeFile();
                }

                resetFileInformation(address);
            }
            _currentState = CLOSING;
        }
    }
    else if (_currentState == OPEN_DATA_WRITE) // received byte to write to open file
    {
        openFileInfo* of = getFileInfoForAddress(_secondaryAddress);
        if (of != NULL)
        {
            _dataSource->getBuffer()[of->_fileBufferIndex++] = rdchar;
            if (!of->_command && of->_fileBufferIndex >= (int)_dataSource->writeBufferSize())
            {
                _dataSource->writeBufferToFile(of->_fileBufferIndex);
                of->_fileBufferIndex = 0;
            }
        }
    }
    else if (_currentState == LOAD_FNAME_READ ||
                _currentState == SAVE_FNAME_READ ||
                _currentState == OPEN_FNAME_READ)
    {
        // add character to filename
        progname[_filenamePosition] = rdchar;
        _filenamePosition++;
        progname[_filenamePosition] = 0;

        if (_ieee->eoi_is_low())
        {
            // this is a directory request
            if (progname[0] == '$' || (progname[0] == '@' && progname[1] == ':'))
            {
                _filenamePosition = 0;
                _currentState = DIR_READ;
                initDirectory();
            }
            else
            {
                // process filename, remove drive indicators and file type
                bool write;
                _filenamePosition = processFilename(progname, _filenamePosition, &write);

                // TODO: figure out at this point if this is a read or write using modifiers in filename
                // this prevents having to try opening the file every time, which is slow.

                const uint8_t *ext;
                if (_currentState == OPEN_FNAME_READ)
                {
                    ext = _seqExtension;
                    if (write == true)
                    {
                        _currentState = OPEN_FNAME_READ_DONE_FOR_WRITING;
                    }
                    else
                    {
                        _currentState = OPEN_FNAME_READ_DONE;
                    }
                }
                else
                {
                    ext = _fileExtension;

                    if (_currentState == LOAD_FNAME_READ)
                    {
                        _currentState = FILE_READ_OPENING;
                    }
                    else if (_currentState == SAVE_FNAME_READ)
                    {
                        _currentState = FILE_SAVE_OPENING;
                    }
                }

                // if not a command, copy the file extension onto the end of the file name
                if (_secondaryAddress != 15)
                {
                    memcpy(&progname[_filenamePosition], (uint8_t*)ext, 5);
                }
                _filenamePosition = 0;
            }
        }
    }

    if (!_ieee->acknowledge_bus_byte()) {
        return;
    }

    // === PREPARE FOR READ/WRITE

    if (_currentState == FILE_READ_OPENING ||
        _currentState == FILE_SAVE_OPENING ||
        _currentState == OPEN_FNAME_READ_DONE ||
        _currentState == OPEN_FNAME_READ_DONE_FOR_WRITING ||
        _currentState == DIR_READ)
    {
        // initialize datasource
        if (!_dataSource->init()) 
        {
            _fileNotFound = 1;
        }

        if (_currentState == FILE_SAVE_OPENING)
        {
            // open file
            _dataSource->openFileForWriting(progname);
        }
        else if (_currentState == FILE_READ_OPENING ||
                 _currentState == OPEN_FNAME_READ_DONE) // file read, either LOAD or OPEN command
        {
            // check for direct access command
            openFileInfo* of = getFileInfoForAddress(_secondaryAddress);
            if (of != NULL && of->_command == true)
            {
                // check for directory commands
                processCommand(progname);
            }
            else if (!openFile(progname))
            {
                log_d("not found, prog: %s", progname);
                // file not found
                _fileNotFound = 1;
            }
            else
            {
                _bytesToSend = _dataSource->getNextFileBlock();
                _bufferFileIndex = 0;

                if (_currentState == OPEN_FNAME_READ_DONE)
                {
                    _fileNotFound = 0;
                    
                    if (of != NULL)
                    {
                        strcpy(of->_fileName, (const char*)progname);
                        of->_fileBufferIndex = 0;
                        of->_useRemainderByte = false;
                        of->_remainderByte = 0;
                        of->_nextByte = _dataSource->getBuffer()[0];
                    }
                    
                    _bufferFileIndex = _secondaryAddress;
                }
            }
        }
        else if (_currentState == OPEN_FNAME_READ_DONE_FOR_WRITING)
        {
            openFileInfo* of = getFileInfoForAddress(_secondaryAddress);
            strcpy(of->_fileName, (const char*)progname);
            of->_fileBufferIndex = -1;
        }
        _currentState = IDLE;
    }

    if ((rdchar == UNLISTEN) || (rdchar == UNTALK && _ieee->atn_is_low()))
    {
        openFileInfo* of = getFileInfoForAddress(_secondaryAddress);
        if (of != NULL && of->_command == true)
        {
            if (of->_fileBufferIndex >= 0) {
                _dataSource->getBuffer()[of->_fileBufferIndex] = 0;
                if (processCommand(_dataSource->getBuffer()))
                {
                    _bufferFileIndex = _secondaryAddress;
                }
                else
                {
                    // retrieve the address from parsing command string
                    _dataSource->processCommandString(&_bufferFileIndex);
                }
                of = getFileInfoForAddress(_bufferFileIndex);
                if (of != NULL)
                {
                    of->_nextByte = _dataSource->getBuffer()[0];
                    of->_fileBufferIndex = 0;
                }
            }

            resetFileInformation(_secondaryAddress);
        }

        // unlisten or untalk command
        _ieee->signal_ready_for_data();
        _ieee->unlisten();
        _currentState = IDLE;
        return;
    }

    _ieee->signal_ready_for_data();

    // LOAD requested
    if (_currentState == FILE_READ || _currentState == OPEN_DATA_READ)
    {
        // ==== STARTING LOAD SEQUENCE
        if (!_ieee->begin_output_start()) {
            return;
        }

        if (_currentState == FILE_READ)
        {
            // get packet
            if (progname[0] == '$' || (progname[0] == '@' && progname[1] == ':'))
            {
                //log_i("dir request");
                // reading a directory
                // need to handle both standard load"$" command and DIRECTORY/CATALOG here
                // on each byte sent, we should check for ATN asserted.
                // if ATN is asserted, we exit and wait for further commands
                bool done = false;
                uint8_t result;
                while (!done)
                {
                    // send one header byte
                    result = _ieee->sendIEEEByteCheckForATN2(_directoryNextByte, _lastDirectoryBlock && _directoryEntryByteIndex == 31);
                    if (result == TIMEOUT) {
                        return;
                    }

                    IEEEBusSignal busSignal = _ieee->wait_for_ndac_high_or_atn_low();
                    if (busSignal == TIMEOUT) {
                        return;
                    }

                    if (busSignal == ATN)
                    {
                        // ATN asserted, break out of directory listing
                        break;
                    }
                    else
                    {
                        _directoryEntryByteIndex++;
                        // read new byte if needed
                        if (_directoryEntryByteIndex >= 32)
                        {
                            if (_directoryEntryIndex == 0)
                            {
                                // do original dir load routine
                                // this is a change directory command
                                if (progname[1] == ':')
                                {
                                    if (progname[0] == '@')
                                    {
                                        int len = strlen((const char*)progname);
                                        sprintf((char*)&progname[len], ".D64");
                                    }

                                    // change directory command
                                    // this can be either a directory name, or a d64 file
                                    if (isD64((const char*)&progname[2]))
                                    {
                                        log_d("d64: %s", &progname[2]);
                                        // this is a d64 file, mount as a datasource
                                        // initialize d64 datasource
                                        bool success = _d64->initWithDataSource(_dataSource, (const char*)&progname[2], NULL);
                                        if (success)
                                        {
                                            setDataSource(_primaryAddress, _d64);
                                            _dataSource = _d64;
                                        }
                                        else
                                        {
                                            log_d("not found %s\n", progname);
                                        }
                                    }
                                    else
                                    {
                                        if (_dataSource == _d64)
                                        {
                                            if (progname[2] == '.' && progname[3] == '.')
                                            {
                                                // unmount this d64 image and return to previous datasource
                                                _dataSource = _d64->getFileDataSource();
                                                setDataSource(_primaryAddress, _dataSource);
                                            }
                                        }
                                        else
                                        {
                                            // change directory
                                            _dataSource->openDirectory((const char*)&progname[2]);
                                        }
                                    }
                                }

                                // have not opened the datasource directory yet
                                // do this here where we are not going to time out
                                _dataSource->openCurrentDirectory();
                            }

                            getDirectoryEntry();
                            _directoryEntryByteIndex = 0;
                        }

                        _ieee->raise_dav_and_eoi();

                        result = _ieee->wait_for_ndac_low_or_atn_low();
                        if (result == TIMEOUT) {
                            return;
                        }

                        if (result == ATN)
                        {
                            _directoryEntryByteIndex--;
                            break;
                        }
                        else
                        {
                            _directoryNextByte = _directoryEntry[_directoryEntryByteIndex];
                        }
                    }
                }
            }
            else // read from file
            {
                bool output_started = false;
                // send blocks of file
                uint8_t done_sending = 0;
                while (done_sending == 0)
                {
                    if (_bytesToSend == 0)
                    {
                        _bytesToSend = _dataSource->getNextFileBlock();
                    }

                    if (_dataSource->isLastBlock())
                    {
                        done_sending = 1;
                    }

                    if (!output_started) {
                        output_started = true;
                    }
                    if (!_ieee->sendIEEEBytes(_dataSource->getBuffer(), _bytesToSend, done_sending)) {
                        return;
                    }
                    _bytesToSend = 0;
                }
            }
        }
        else if (_currentState == OPEN_DATA_READ)
        {
            bool done = false;
            uint8_t result = 0;
            
            uint8_t address = rdchar & PET_ADDRESS_MASK;
            openFileInfo* of = getFileInfoForAddress(address);
            if (address == 15)
            {
                // read from address 15 is the drive status
                of->_nextByte = '\r';
            }
            while (!done)
            {
                if (of->_useRemainderByte == true)
                {
                    result = _ieee->sendIEEEByteCheckForATN(of->_remainderByte);
                }
                else
                {
                    result = _ieee->sendIEEEByteCheckForATN(of->_nextByte);
                }

                result = _ieee->wait_for_ndac_high_or_atn_low();
                if (result == TIMEOUT) {
                    return;
                }

                if (result == ATN)
                {
                    done = true;
                }
                else
                {
                    if (of->_useRemainderByte == true)
                    {
                        of->_useRemainderByte = false;
                    }
                    else
                    {
                        of->_remainderByte = _dataSource->getBuffer()[of->_fileBufferIndex];
                        of->_fileBufferIndex++;
                        of->_byteIndex++;
                    }

                    if (_bufferFileIndex != address)
                    {
                        _dataSource->openFileForReading((uint8_t*)of->_fileName);
                        int blocksToRead = (of->_byteIndex / _dataSource->readBufferSize()) + 1;

                        // read enough blocks to get back to the right block of the file
                        for (int i = 0; i < blocksToRead; i++)
                        {
                            _dataSource->getNextFileBlock();
                        }
                        _bufferFileIndex = address;
                    }

                    if (of->_fileBufferIndex >= (int)_dataSource->readBufferSize())
                    {
                        // get next buffer block
                        _bytesToSend = _dataSource->getNextFileBlock();
                        of->_fileBufferIndex = 0;
                    }

                    _ieee->raise_dav_and_eoi();

                    result = _ieee->wait_for_ndac_low_or_atn_low();
                    if (result == TIMEOUT) {
                        return;
                    }
                    
                    if (result == ATN)
                    {
                        if (of->_fileBufferIndex == 0)
                        {
                            of->_useRemainderByte = true;
                        }
                        else
                        {
                            of->_fileBufferIndex--;
                        }

                        done = true;
                    }

                    of->_nextByte = _dataSource->getBuffer()[of->_fileBufferIndex];
                }
            }
        }

        // ==== ENDING LOAD SEQUENCE
        _ieee->end_output();
        _currentState = IDLE;

    }
    else if (_currentState == FILE_SAVE)
    {
        // save command
        if (!writeFile()) {
            return;
        }
        _ieee->unlisten();
        _currentState = IDLE;
    }
}

uint8_t PETdisk::processFilename(uint8_t* filename, uint8_t length, bool* write)
{
    *write = false;
    if (_secondaryAddress == 15)
    {
        return length;
    }
    
    uint8_t drive_separator = ':';
    uint8_t* sepptr = (uint8_t*)bf_memmem(filename, length, &drive_separator, 1);
    if (sepptr)
    {
        // found a drive separator
        int seplen = sepptr - filename + 1;
        memmove(filename, sepptr + 1, length - seplen);
        length -= seplen;
    }

    // find part of string before ','
    drive_separator = ',';
    sepptr = (uint8_t*)bf_memmem(filename, length, &drive_separator, 1);
    if (sepptr)
    {
        // look for write indicator
        uint8_t writeIndicator[2];
        writeIndicator[0] = ',';
        writeIndicator[1] = 'W';
        int fnameLength = sepptr - filename;
        int indicatorsSize = length - fnameLength;

        if (bf_memmem(sepptr, indicatorsSize, writeIndicator, 2) != NULL)
        {
            // mark as a write
            *write = true;
        }

        length = fnameLength;
    }

    return length;
}

bool PETdisk::processCommand(uint8_t* command)
{
    int len = strlen((char*)command);
    if (len < 3)
    {
        return false;
    }

    if (toupper(command[0]) == 'C' && toupper(command[1]) == 'D')
    {
        // change directory command
        if (command[2] == DIR_BACK_CHARACTER)
        {
            if (_dataSource == _d64)
            {
                // unmount this d64 image and return to previous datasource
                _dataSource = _d64->getFileDataSource();
                setDataSource(_primaryAddress, _dataSource);
            }
            else
            {
                char d[3];
                sprintf(d, "..");
                _dataSource->openDirectory(d);
            }
            return true;
        }

        if (command[2] != ':')
        {
            return false;
        }

        if (len < 4)
        {
            return false;
        }

        char dirname[64];
        strcpy(dirname, (char*)&command[3]);

        if (isD64(dirname))
        {
            // cd command to d64 file
            bool success = _d64->initWithDataSource(_dataSource, dirname, NULL);
            if (success)
            {
                setDataSource(_primaryAddress, _d64);
                _dataSource = _d64;
            }
            else
            {
                log_d("not found: %s\n", dirname);
            }
        }
        else
        {
            // change directory
            log_d("open dir: %s\n", dirname);
            if (_dataSource->openDirectory(dirname))
            {
                log_d("opened.\n");
            }
            else
            {
                log_d("not opened\n");
            }
        }

        return true;
    }

    return false;
}

bSPI _spi;
SD _sd;
bitfixer::FAT32 _fat32;
bitfixer::EspConn _espConn;
bitfixer::EspHttp _espHttp;
D64DataSource _d64DataSource;
PETdisk _petdisk;

bitfixer::NetworkDataSource nds0;
bitfixer::NetworkDataSource nds1;
bitfixer::NetworkDataSource nds2;
bitfixer::NetworkDataSource nds3;

void run_diagnostics() {
    DataSource* ds = _petdisk.getDataSource(10);
    if (!ds) {
        log_i("diag: no datasource");
        return;
    }

    log_i("open for writing");
    char fname[32];
    sprintf(fname, "test.abc");
    ds->openFileForWriting((uint8_t*)fname);
    log_i("get buffer");
    uint8_t* buf = ds->getBuffer();
    memset(buf, 0xAA, 256);
    buf[100] = 0x00;
    ds->writeBufferToFile(256);
    ds->closeFile();
    log_i("done");
}

void setup()
{
    _bufferSize = 0;
    prog_init();
    _spi.init();
    _sd.initWithSPI(&_spi, spi_cs());
    _fat32.initWithParams(&_sd, _buffer, &_buffer[512]);

    bitfixer::IEEE488* ieee = bitfixer::IEEE488::get_instance();
    
    if (checkForDisable((char*)&_buffer[769], &_fat32))
    {
        log_i("DISABLE.PD found, disabling device\n");
        ieee->init();
        ieee->unlisten();
        init_led();
        while(1) {
            blink_led(4, 500, 500);
            hDelayMs(1000);
        }
    }
    checkForFirmware((char*)&_buffer[769], &_fat32);

    init_led();
    blink_led(1, 300, 50);

    _espConn.initWithParams(_buffer, &_bufferSize);
    _espHttp.initWithParams(&_espConn);

    ieee->init();
    ieee->unlisten();
    
    nds0.initWithParams(&_espHttp, _buffer, &_bufferSize);
    nds1.initWithParams(&_espHttp, _buffer, &_bufferSize);
    nds2.initWithParams(&_espHttp, _buffer, &_bufferSize);
    nds3.initWithParams(&_espHttp, _buffer, &_bufferSize);

    bitfixer::NetworkDataSource* nds_array[4];
    nds_array[0] = &nds0;
    nds_array[1] = &nds1;
    nds_array[2] = &nds2;
    nds_array[3] = &nds3;

    _petdisk.init(
        &_fat32,
        &_d64DataSource,
        _buffer, 
        &_bufferSize, 
        &_espConn, 
        &_espHttp, 
        (bitfixer::NetworkDataSource**)nds_array,
        ieee);

    // run diagnostics if needed
    // TODO: check if DIAG.PD2 exists
    // run_diagnostics();

    log_i("ready");
    set_led(true);
}

void loop()
{
    _petdisk.loop();
}

TaskHandle_t loopTaskHandle = NULL;

void loopTask(void *pvParameters)
{
    setup();
    for(;;) {
        loop();
    }
}

extern "C" void app_main() {
    esp_log_level_set("pd", ESP_LOG_INFO);
    gpio_init();
    // select between test mode and run mode
    Console::init();
    hardware_cmd_init();
    xTaskCreatePinnedToCore(loopTask, "loopTask", 4096, NULL, 20, &loopTaskHandle, 0);
    setup_atn_interrupt();
}