#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include "EspConn.h"
#include "Buffer.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "Serial.h"
#include "SerialLogger.h"
#include "EspHttp.h"
#include "SD_routines.h"
#include "SPI_routines.h"
#include "FAT32.h"
#include "IEEE488.h"
#include "DataSource.h"
#include "NetworkDataSource.h"
#include "githash.h"

#define ESP_CONTROL     DDRD
#define ESP_PORT        PORTD
#define ESP_CH_PD       PD4
#define ESP_RST         PD5
#define ESP_GPIO0       PD7
#define ESP_GPIO2       PD6

#define LED_CONTROL     DDRB
#define LED_PORT        PORTB
#define LED_PIN1        PB0
#define LED_PIN2        PB1

// addresses for PET IEEE commands
#define PET_LOAD_FNAME_ADDR     0xF0
#define PET_SAVE_FNAME_ADDR     0xF1
#define PET_OPEN_FNAME_MASK     0xF0
#define PET_READ_CMD_ADDR       0x60
#define PET_SAVE_CMD_ADDR       0x61
#define PET_OPEN_IO_ADDR        0x60

const unsigned char _dirHeader[] PROGMEM =
{
    0x01,
    0x04,
    0x1F,
    0x04,
    0x00,
    0x00,
    0x12,
};

const unsigned char _versionString[] PROGMEM = "\"PETDISK MAX V1.0\"      ";
const unsigned char _firmwareString[] PROGMEM = "BUILD ";
const unsigned char _fileExtension[] PROGMEM =
{
    '.',
    'P',
    'R',
    'G',
    0x00,
};

const unsigned char _seqExtension[] PROGMEM =
{
    '.',
    'S',
    'E',
    'Q',
    0x00,
};

void pgm_memcpy(unsigned char *dest, unsigned char *src, int len)
{
    int i;
    for (i = 0; i < len; i++)
    {
        *dest++ = pgm_read_byte(&(*src++));
    }
}

void prog_init()
{
    // set reset for esp
    ESP_PORT = (1 << ESP_CH_PD) | (1 << ESP_RST);
    ESP_CONTROL = (1 << ESP_CH_PD) | (1 << ESP_RST);
    sei();
}
 
void reset_esp()
{
    ESP_PORT &= ~(1 << ESP_RST);
    ESP_PORT &= ~(1 << ESP_CH_PD);

    for (int i = 0; i < 20; i++)
    {
        _delay_loop_2(65535);
    }

    ESP_PORT |= 1 << ESP_RST;
    ESP_PORT |= 1 << ESP_CH_PD;
}

void init_led()
{
    LED_CONTROL |= 1 << LED_PIN1;
    LED_CONTROL |= 1 << LED_PIN2;
}

void set_led(bool value)
{
    if (value == true)
    {
        LED_PORT |= 1 << LED_PIN1;
        LED_PORT |= 1 << LED_PIN2;
    }
    else
    {
        LED_PORT &= ~(1 << LED_PIN1);
        LED_PORT &= ~(1 << LED_PIN2);
    }
}

void blink_led(int count, int ms_on, int ms_off)
{
    set_led(false);
    for (int i = 0; i < count; i++)
    {
        set_led(true);
        for (int j = 0; j < ms_on; j++)
        {
            _delay_ms(1);
        }
        set_led(false);
        for (int j = 0; j < ms_off; j++)
        {
            _delay_ms(1);
        }
    }
}

void checkForFirmware(char* buffer, FAT32* fat32, Serial1* log)
{
    log->transmitStringF(PSTR("checking\r\n"));
    if (!fat32->init())
    {
        log->transmitStringF(PSTR("noinit\r\n"));
        return;
    }

    sprintf_P(buffer, PSTR("FIRM*"));
    fat32->openCurrentDirectory();
    if (!fat32->findFile(buffer))
    {
        log->transmitStringF(PSTR("nofirm\r\n"));
        return;
    }

    log->transmitStringF(PSTR("gotfirmware\r\n"));
    // found firmware file, delete
    fat32->deleteFile();
}

void sd_test(DataSource* dataSource, Serial1* log)
{
    if (!dataSource->init())
    {
        log->transmitStringF(PSTR("no init\r\n"));
    }

    log->transmitStringF(PSTR("init\r\n"));
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

struct pd_config {
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
    }
    ~PETdisk() {}

    void init(
        FAT32* fat32, 
        Serial1* log, 
        uint8_t* buffer, 
        uint16_t* bufferSize, 
        EspConn* espConn, 
        EspHttp* espHttp, 
        NetworkDataSource** nds_array,
        IEEE488* ieee,
        SerialLogger* logger);
    void setDataSource(unsigned char id, DataSource* dataSource);
    DataSource* getDataSource(unsigned char id);

    bool readConfigFile(FAT32* fat32, Serial1* log, uint8_t* buffer);
    void printConfig(struct pd_config* pdcfg, Serial1* log);
    void run();

private:
    DataSource* _dataSources[8];
    EspConn* _espConn;
    EspHttp* _espHttp;
    IEEE488* _ieee;
    FAT32* _fat32;
    SerialLogger* _logger;

    DataSource* _dataSource;
    unsigned char buscmd;
    unsigned char ieee_address;
    unsigned char rdchar;
    unsigned char* progname;
    int filename_position;
    int filenotfound;
    unsigned char getting_filename;
    unsigned char savefile;
    unsigned char gotname;
    unsigned char done_sending;
    bool init_datasource;
    int bytes_to_send;
    pdstate _currentState;
    unsigned char _openFileAddress;
    int _fileWriteByte;
    filedir _fileDirection;
    int _fileReadByte;
    int _useRemainderByte;
    unsigned char _remainderByte;

    bool configChanged(struct pd_config* pdcfg);
    unsigned char processFilename(unsigned char* filename, unsigned char length);
    void writeFile();
    void listFiles();
    unsigned char wait_for_device_address();
    void run2();
};

void PETdisk::init(
    FAT32* fat32, 
    Serial1* log, 
    uint8_t* buffer, 
    uint16_t* bufferSize, 
    EspConn* espConn, 
    EspHttp* espHttp, 
    NetworkDataSource** nds_array,
    IEEE488* ieee,
    SerialLogger* logger)
{
    _fat32 = fat32;
    _ieee = ieee;
    _logger = logger;

    // reset state variables
    _dataSource = 0;
    progname = (unsigned char*)&_buffer[1024-64];
    filename_position = 0;
    filenotfound = 0;
    getting_filename = 0;
    savefile = 0;
    gotname = 0;
    done_sending = 0;
    init_datasource = false;
    bytes_to_send = 0;
    _currentState = IDLE;
    _openFileAddress = 0;
    _fileWriteByte = 0;
    _fileDirection = FNONE;
    _fileReadByte = 0;
    _useRemainderByte = 0;
    _remainderByte = 0;

    char tmp[32];
    if (readConfigFile(fat32, log, buffer))
    {
        log->transmitStringF(PSTR("read config file\r\n"));
    }
    else
    {
        log->transmitStringF(PSTR("no config read\r\n"));
    }

    struct pd_config *pdcfg = (struct pd_config*)&buffer[512];
    eeprom_read_block(pdcfg, (void*)0, sizeof(struct pd_config));
    printConfig(pdcfg, log);

    // check validity of config
    if (pdcfg->device_type[0] > DEVICE_END)
    {
        // config not set
        // use defaults
        setDataSource(8, fat32);
        log->transmitStringF(PSTR("using default\r\n"));
        return;
    }

    _espConn = espConn;
    _espHttp = espHttp;

    bool espConnected = false;

    if (strlen(pdcfg->wifi_ssid) > 0 && strlen(pdcfg->wifi_password) > 0)
    {
        log->transmitStringF(PSTR("trying to connect\r\n"));
        
        bool device_present = true;
        if (!_espConn->device_present())
        {
            log->transmitStringF(PSTR("no device!\r\n"));
            _espConn->attempt_baud_rate_setting();
            if (_espConn->device_present())
            {
                log->transmitStringF(PSTR("device present at 115kbps\r\n"));
            }
            else
            {
                log->transmitStringF(PSTR("no device present.\r\n"));
                device_present = false;
            }
        }
        
        if (device_present)
        {
            log->transmitStringF(PSTR("device present\r\n"));
            if (_espConn->init())
            {
                if (_espConn->connect(pdcfg->wifi_ssid, pdcfg->wifi_password))
                {
                    _espConn->setDns();
                    espConnected = true;
                    log->transmitStringF(PSTR("connected\r\n"));
                    blink_led(2, 150, 150);
                }
                else
                {
                    log->transmitStringF(PSTR("no connect\r\n"));
                    blink_led(5, 150, 150);
                }
            }
            else
            {
                blink_led(4, 150, 150);
                log->transmitStringF(PSTR("could not connect\r\n"));
            }
        }
        else
        {
            blink_led(5, 150, 150);
        }
    }

    // check each entry in config file
    int network_drive_count = 0;
    for (int i = 0; i < 9; i++)
    {
        sprintf_P(tmp, PSTR("i %d dt %d\r\n"), i, pdcfg->device_type[i]);
        log->transmitString(tmp);
        if (pdcfg->device_type[i] == DEVICE_SD0)
        {
            setDataSource(i + MIN_DEVICE_ID, fat32);
            log->transmitStringF(PSTR("SD0\r\n"));
        }
        else if (pdcfg->device_type[i] >= DEVICE_URL_BASE && network_drive_count < 4)
        {
            int device_id = i + MIN_DEVICE_ID;
            int url_index = pdcfg->device_type[i] - DEVICE_URL_BASE;
            int eeprom_offset = 9 + (64 * url_index);

            // find the host and url portion for this datasource
            char* url = strchr(pdcfg->urls[url_index], '/');
            int url_offset = url - pdcfg->urls[url_index];

            NetworkDataSource* nds = nds_array[network_drive_count];

            nds->setUrlData(
                (void*)eeprom_offset, 
                url_offset, 
                (void*)(eeprom_offset+url_offset), 
                strlen(pdcfg->urls[url_index]) - url_offset);

            sprintf_P(tmp, PSTR("d %d %d %d\r\n"), device_id, eeprom_offset, url_offset);
            log->transmitString(tmp);
            
            if (espConnected)
            {
                setDataSource(device_id, nds);
            }
            else
            {
                setDataSource(device_id, 0);
            }

            network_drive_count++;
        }
    }
}

void PETdisk::printConfig(struct pd_config* pdcfg, Serial1* log)
{
    char tmp[16];
    for (int i = 0; i < 9; i++)
    {
        sprintf_P(tmp, PSTR("d %d->%d\r\n"), i, pdcfg->device_type[i]);
        log->transmitString(tmp);
    }

    for (int i = 0; i < 4; i++)
    {
        sprintf_P(tmp, PSTR("u %d->"), i);
        log->transmitString(tmp);
        log->transmitString(pdcfg->urls[i]);
        log->transmitStringF(PSTR("\r\n"));
    }

    log->transmitStringF(PSTR("ssid: "));
    log->transmitString(pdcfg->wifi_ssid);
    log->transmitStringF(PSTR("\r\n"));

    log->transmitStringF(PSTR("ssid: "));
    log->transmitString(pdcfg->wifi_password);
    log->transmitStringF(PSTR("\r\n"));
}

bool PETdisk::configChanged(struct pd_config* pdcfg)
{
    // check pdconfig against version in eeprom
    uint8_t byte;
    uint8_t* cfg = (uint8_t*)pdcfg;
    for (int i = 0; i < (int)sizeof(struct pd_config); i++)
    {
        byte = eeprom_read_byte((const uint8_t*)i);
        if (byte != cfg[i])
        {
            return true;
        }
    }
    return false;
}

bool PETdisk::readConfigFile(FAT32* fat32, Serial1* log, uint8_t* buffer)
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
    if (!fat32->openFileForReading((unsigned char*)cfg_fname))
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

    token = (char*)buffer;
    next = strchr((char*)buffer, '\n');
    while (token != 0)
    {
        sscanf_P(token, PSTR("%[^,],%s\n"), str[0], str[1]);
        id = atoi(str[0]);
        if (id == 0)
        {
            if (str[0][0] == 's' || str[0][0] == 'S')
            {
                // use next field as ssid
                strcpy(pdcfg->wifi_ssid, str[1]);
            }
            else if (str[0][0] == 'p' || str[0][0] == 'P')
            {
                strcpy(pdcfg->wifi_password, str[1]);
            }
        }
        else
        {
            if (id < MIN_DEVICE_ID || id > MAX_DEVICE_ID)
            {
                log->transmitStringF("invalid field\r\n");
                return false;
            }

            // check if this is an sd card or a url string
            if (str[1][0] == 'S' && str[1][1] == 'D')
            {
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
                    log->transmitStringF("invalid sd\r\n");
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
        log->transmitStringF(PSTR("config updated\r\n"));
        eeprom_write_block(pdcfg, (void*)0, sizeof(struct pd_config));
        blink_led(3, 150, 150);
    }
    else
    {
        log->transmitStringF(PSTR("config not updated\r\n"));
        blink_led(4, 150, 150);
    }

    return true;
}

void PETdisk::setDataSource(unsigned char id, DataSource* dataSource)
{
    if (id >= MIN_DEVICE_ID && id <= MAX_DEVICE_ID)
    {
        _dataSources[id-MIN_DEVICE_ID] = dataSource;
    }
}

DataSource* PETdisk::getDataSource(unsigned char id)
{
    if (id < MIN_DEVICE_ID || id > MAX_DEVICE_ID)
    {
        return 0;
    }

    return _dataSources[id-MIN_DEVICE_ID];
}

void PETdisk::writeFile()
{
    unsigned int numBytes;
    unsigned char rdchar;
    unsigned char* dataBuffer = _dataSource->getBuffer();
    unsigned int writeBufferSize = _dataSource->writeBufferSize();
    
    numBytes = 0;
    do
    {
        rdchar = _ieee->get_byte_from_bus();
        dataBuffer[numBytes++] = rdchar;
        
        if (numBytes >= writeBufferSize)
        {
            _dataSource->writeBufferToFile(numBytes);
            numBytes = 0;
        }
        
        _ieee->acknowledge_bus_byte();
        _ieee->signal_ready_for_data();
    }
    while (!_ieee->eoi_is_low());
    
    if (numBytes > 0)
    {
        _dataSource->writeBufferToFile(numBytes);
    }
    
    _dataSource->closeFile();
}

void PETdisk::listFiles()
{
    bool gotDir;
    unsigned char startline;
    unsigned int dir_start;
    unsigned char entry[32];
    dir_start = 0x041f;
    unsigned int file = 0;

    //log->transmitStringF(PSTR("open dir\r\n"));
    _dataSource->openCurrentDirectory();

    do
    {
        // get next directory entry
        gotDir = _dataSource->getNextDirectoryEntry();
        if (gotDir == false)
        {
            dir_start += 0x0020;
            startline = 0;
            memset(entry, ' ', 32);
            entry[startline] = (unsigned char)(dir_start & 0x00ff);
            entry[startline+1] = (unsigned char)((dir_start & 0xff00) >> 8);
            entry[startline+2] = file+1;
            entry[startline+3] = 0x00;
            entry[startline+4] = 0x20;
            entry[startline+5] = 0x20;
            pgm_memcpy(&entry[startline+6], (unsigned char*)_firmwareString, 6);
            pgm_memcpy(&entry[startline+6+6], (unsigned char*)_hash, 7);
            entry[startline+31] = 0x00;
            file++;
            _ieee->sendIEEEBytes(entry, 32, 0);

            startline = 0;
            dir_start += 0x001e;
            entry[startline] = (unsigned char)(dir_start & 0x00ff);
            entry[startline+1] = (unsigned char)((dir_start & 0xff00) >> 8);
            entry[startline+2] = 0xff;
            entry[startline+3] = 0xff;
            sprintf_P((char *)&entry[startline+4], PSTR("BLOCKS FREE.             "));
            entry[startline+29] = 0x00;
            entry[startline+30] = 0x00;
            entry[startline+31] = 0x00;
            
            _ieee->sendIEEEBytes(entry, 32, 1);
            return;
        }
        else
        {
            if (!_dataSource->isHidden() && !_dataSource->isVolumeId())
            {
                // check if this is a file that can be used by petdisk
                // currently .PRG files. Soon .SEQ and .REL

                int fname_length = 0;
                unsigned char* fileName = _dataSource->getFilename();
                fname_length = strlen((char*)fileName);
                bool valid_file = false;
                // check for correct extension
                if (fname_length > 4)
                {
                    if (toupper(fileName[fname_length-3]) == 'P' &&
                        toupper(fileName[fname_length-2]) == 'R' &&
                        toupper(fileName[fname_length-1]) == 'G')
                    {
                        valid_file = true;
                    }
                }

                if (!valid_file)
                {
                    // skip this file, continue to next
                    continue;
                }

                dir_start += 0x0020;
                startline = 0;

                
                entry[startline] = (unsigned char)(dir_start & 0x00ff);
                entry[startline+1] = (unsigned char)((dir_start & 0xff00) >> 8);
                entry[startline+2] = file+1;
                entry[startline+3] = 0x00;
                entry[startline+4] = 0x20;
                entry[startline+5] = 0x20;
                entry[startline+6] = 0x22;

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
                    entry[startline+7+f] = toupper(fileName[f]);
                }

                entry[startline+7+fname_length] = 0x22;
                for (int f = 0; f < (17 - fname_length); f++)
                {
                    entry[startline+7+fname_length+f+1] = ' ';
                }

                if (_dataSource->isDirectory())
                {
                    entry[startline+25] = 'D';
                    entry[startline+26] = 'I';
                    entry[startline+27] = 'R';
                }
                else
                {
                    if (extensionPos > 0)
                    {
                        // make sure extension is upper case
                        entry[startline+25] = toupper(fileName[extensionPos]);
                        entry[startline+26] = toupper(fileName[extensionPos+1]);
                        entry[startline+27] = toupper(fileName[extensionPos+2]);
                    }
                    else
                    {
                        entry[startline+25] = ' ';
                        entry[startline+26] = ' ';
                        entry[startline+27] = ' ';
                    }
                }

                entry[startline+28] = ' ';
                entry[startline+29] = ' ';
                entry[startline+30] = ' ';
                entry[startline+31] = 0x00;
                file++;

                _ieee->sendIEEEBytes(entry, 32, 0);
            }
        }
    }
    while (gotDir == true);
}

unsigned char PETdisk::wait_for_device_address()
{
    unsigned char ieee_address;
    _dataSource = 0;
    while (_dataSource == 0)
    {
        ieee_address = _ieee->get_device_address(&buscmd);
        _dataSource = getDataSource(ieee_address);

        if (_dataSource == 0)
        {
            _ieee->reject_address();
            continue;
        }

        _ieee->accept_address();
    }
    return buscmd;
}

void PETdisk::run()
{
    // start main loop
    while(1)
    {
        if (_currentState == FILE_NOT_FOUND)
        {
            _ieee->unlisten();
            _currentState = IDLE;
            filenotfound = 0;
            filename_position = 0;
            memset(progname, 0, 64);
        }

        if (IEEE_CTL == 0x00)
        {
            // if we are in an unlisten state,
            // wait for my address
            buscmd = wait_for_device_address();
            if (buscmd == LISTEN)
            {
                _currentState = BUS_LISTEN;
            }
            else
            {
                _currentState = BUS_TALK;
            }
        }

        rdchar = _ieee->get_byte_from_bus();

        if (_ieee->atn_is_low()) // check for bus command
        {
            if (rdchar == PET_LOAD_FNAME_ADDR)
            {
                _currentState = LOAD_FNAME_READ;
            }
            else if (rdchar == PET_SAVE_FNAME_ADDR)
            {
                _currentState = SAVE_FNAME_READ;
            }
            else if ((rdchar & 0xF0) == PET_OPEN_FNAME_MASK) // open command to another address
            {
                _currentState = OPEN_FNAME_READ;
                _openFileAddress = (rdchar & 0x0F);
                _fileWriteByte = -1;
            }
            else if (rdchar == PET_READ_CMD_ADDR) // read command
            {
                if (filenotfound == 1)
                {
                    _currentState = FILE_NOT_FOUND;
                }
                else
                {
                    _currentState = FILE_READ;
                    // open file for reading
                    if (progname[0] == '$')
                    {
                        // copy the directory header
                        pgm_memcpy((unsigned char *)_buffer, (unsigned char *)_dirHeader, 7);

                        // print directory title
                        pgm_memcpy((unsigned char *)&_buffer[7], (unsigned char *)_versionString, 24);
                        _buffer[31] = 0x00;
                    }
                }
            }
            else if (rdchar == PET_SAVE_CMD_ADDR) // save command
            {
                _currentState = FILE_SAVE;
            }
            else if ((rdchar & 0xF0) == PET_OPEN_IO_ADDR) // print or input command
            {
                unsigned char temp = rdchar & 0x0F;
                if (temp == _openFileAddress)
                {
                    if (_currentState == BUS_LISTEN)
                    {
                        if (_fileWriteByte == -1)
                        {
                            //transmitString_F(_saving);
                            _logger->log("saving\r\n");
                            _dataSource->openFileForWriting(progname);
                            _fileWriteByte = 0;
                        }
                        _fileDirection = FWRITE;
                        _currentState = OPEN_DATA_WRITE;
                    }
                    else
                    {
                        if (filenotfound == 1)
                        {
                            _fileDirection = FNONE;
                            _currentState = FILE_NOT_FOUND;
                        }
                        else
                        {
                            _fileDirection = FREAD;
                            _currentState = OPEN_DATA_READ;
                        }
                    }
                }
            }
            else if ((rdchar & 0xF0) == 0xE0)
            {
                unsigned char temp = rdchar & 0x0F;
                if (temp == _openFileAddress && _fileDirection == FWRITE)
                {
                    if (_fileWriteByte > 0)
                    {
                        _dataSource->writeBufferToFile(_fileWriteByte);
                        _fileWriteByte = 0;
                    }

                    _dataSource->closeFile();
                }

                _openFileAddress = -1;
                _fileWriteByte = -1;
                _fileDirection = FNONE;
                _currentState = CLOSING;
            }
        }
        else if (_currentState == OPEN_DATA_WRITE) // received byte to write to open file
        {
            _buffer[_fileWriteByte++] = rdchar;
            if (_fileWriteByte >= 512)
            {
                _dataSource->writeBufferToFile(_fileWriteByte);
                _fileWriteByte = 0;
            }
        }
        else if (_currentState == LOAD_FNAME_READ ||
                 _currentState == SAVE_FNAME_READ ||
                 _currentState == OPEN_FNAME_READ)
        {
            // add character to filename
            progname[filename_position] = rdchar;
            filename_position++;
            progname[filename_position] = 0;

            //if ((rdbus & EOI) == 0)
            if (_ieee->eoi_is_low())
            {
                // this is a directory request
                if (progname[0] == '$')
                {
                    filename_position = 0;
                    _currentState = DIR_READ;
                }
                else
                {
                    // process filename, remove drive indicators and file type
                    filename_position = processFilename(progname, filename_position);

                    const unsigned char *ext;
                    if (_currentState == OPEN_FNAME_READ)
                    {
                        ext = _seqExtension;
                        _currentState = OPEN_FNAME_READ_DONE;
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

                    // copy the PRG file extension onto the end of the file name
                    pgm_memcpy(&progname[filename_position], (unsigned char*)ext, 5);
                    filename_position = 0;
                    //transmitString(progname);
                    _logger->log((const char*)progname);
                }
            }
        }

        /*
        // raise NDAC
        PORTC = NOT_NRFD;
        wait_for_dav_high();
        // open file if needed
        */
        _ieee->acknowledge_bus_byte();

        //_ieee->signal_ready_for_data();

        // === PREPARE FOR READ/WRITE
        // === re-init sd card and open file

        if (_currentState == FILE_READ_OPENING ||
            _currentState == FILE_SAVE_OPENING ||
            _currentState == OPEN_FNAME_READ_DONE ||
            _currentState == DIR_READ)
        {
            // initialize sd card
            //error = _dataSource->initializeStorage();
            _dataSource->init();

            if (_currentState == FILE_SAVE_OPENING)
            {
                // open file
                _dataSource->openFileForWriting(progname);
                _currentState = IDLE;
            }
            else if (_currentState == FILE_READ_OPENING ||
                     _currentState == OPEN_FNAME_READ_DONE) // file read, either LOAD or OPEN command
            {
                if (!_dataSource->openFileForReading(progname))
                {
                    // file not found
                    filenotfound = 1;
                }
                else
                {
                    if (_currentState == OPEN_FNAME_READ_DONE)
                    {
                        bytes_to_send = _dataSource->getNextFileBlock();
                    }
                    else
                    {
                        // test
                        bytes_to_send = _dataSource->getNextFileBlock();
                    }

                    _fileReadByte = 0;
                    filenotfound = 0;
                    _useRemainderByte = 0;
                    _remainderByte = 0;
                }

                _currentState = IDLE;
            }
        }

        if ((rdchar == UNLISTEN) || (rdchar == UNTALK && _ieee->atn_is_low()))
        {
            // unlisten or untalk command
            //PORTC = NOT_NDAC;
            //unlisten();
            _ieee->signal_ready_for_data();
            _ieee->unlisten();
            _currentState = IDLE;
            continue;
        }

        _ieee->signal_ready_for_data();

        // LOAD requested
        if (_currentState == FILE_READ || _currentState == OPEN_DATA_READ)
        {
            // ==== STARTING LOAD SEQUENCE

            // release NRFD/NDAC
            /*
            DDRC = NDAC;

            // wait for atn high
            wait_for_atn_high();

            DDRC = DAV | EOI;
            PORTC = 0xFF;

            // change data bus to output
            DATA_CTL = 0xff;
            DDRB = DDRB | (DATA0 | DATA1);

            wait_for_ndac_low();
            */
            _ieee->begin_output();

            if (_currentState == FILE_READ)
            {
                // get packet
                if (progname[0] == '$')
                {
                    // reading a directory
                    _ieee->sendIEEEBytes((unsigned char *)_buffer, 32, 0);

                    // this is a change directory command
                    if (progname[1] == ':')
                    {
                        // change directory command
                        _dataSource->openDirectory((const char*)&progname[2]);
                    }
                    // write directory entries
                    //ListFilesIEEE(ds, (unsigned char*)_buffer);
                    listFiles();
                }
                else // read from file
                {
                    // send blocks of file
                    done_sending = 0;

                    /*
                    // test
                    int currByte = 0;
                    int sizeBytes = ds->getFileSize();

                    while (doneSending == 0)
                    {
                        // get next block of the file being read
                        if (bytes_to_send == 0)
                        {
                            bytes_to_send = _dataSource->getNextFileBlock((unsigned char*)_buffer);
                        }

                        if (currByte + bytes_to_send >= sizeBytes)
                        {
                            done_sending = 1;
                            bytes_to_send = sizeBytes - currByte;
                            ds->closeFile();
                        }
                        currByte += bytes_to_send;

                        sendIEEEBytes((unsigned char *)_buffer, bytes_to_send, doneSending);
                        bytes_to_send = 0;
                    }
                    */
                    _logger->logF(PSTR("sending\r\n"));
                    char tmp[8];
                    while (done_sending == 0)
                    {
                        if (bytes_to_send == 0)
                        {
                            bytes_to_send = _dataSource->getNextFileBlock();
                        }

                        if (_dataSource->isLastBlock())
                        {
                            done_sending = 1;
                        }

                        sprintf(tmp, "s %d\r\n", bytes_to_send);
                        _logger->log(tmp);
                        _ieee->sendIEEEBytes(_dataSource->getBuffer(), bytes_to_send, done_sending);
                        bytes_to_send = 0;
                    }
                    _logger->logF(PSTR("done\r\n"));
                }
            }
            else if (_currentState == OPEN_DATA_READ)
            {
                /*
                bool done = false;
                bool found = false;
                unsigned char temp = 0;
                unsigned char result = 0;

                while (!done)
                {
                    if (stateVars.useRemainderByte == 1)
                        result = sendIEEEByteCheckForATN(stateVars.remainderByte);
                    else
                        result = sendIEEEByteCheckForATN(_buffer[stateVars.fileReadByte]);

                    result = wait_for_ndac_high_or_atn_low();

                    if (result == ATN)
                    {
                        done = true;
                    }
                    else
                    {
                        if (_useRemainderByte == 1)
                        {
                            _useRemainderByte = 0;
                        }
                        else
                        {
                            _remainderByte = _buffer[_fileReadByte];
                            _fileReadByte++;
                        }

                        if (_fileReadByte >= 512)
                        {
                            // get next buffer block
                            bytes_to_send = ds->getNextFileBlock((unsigned char*)_buffer);
                            _fileReadByte = 0;
                        }

                        // raise DAV
                        temp = DAV | EOI;
                        // output to bus
                        PORTC = temp;

                        result = wait_for_ndac_low_or_atn_low();

                        if (result == ATN)
                        {
                            if (stateVars.fileReadByte == 0)
                            {
                                stateVars.useRemainderByte = 1;
                            }
                            else
                            {
                                stateVars.fileReadByte--;
                            }

                            done = true;
                        }
                    }
                }
                */
            }

            // ==== ENDING LOAD SEQUENCE
            _ieee->end_output();
            _currentState = IDLE;

        }
        else if (_currentState == FILE_SAVE)
        {
            // save command
            // todo: fix this
            writeFile();
            _ieee->unlisten();
            _currentState = IDLE;
        }
    }
}

void PETdisk::run2()
{
    // main loop
    while(1) 
    {
        if (_currentState == FILE_NOT_FOUND)
        {
            _ieee->unlisten();
            _currentState = IDLE;
            filenotfound = 0;
        }

        if (IEEE_CTL == 0x00) // unlistened
        {
            // not currently listening on the bus.
            // waiting for a valid device address

            _dataSource = 0;
            while (_dataSource == 0)
            {
                ieee_address = _ieee->get_device_address(&buscmd);
                _dataSource = getDataSource(ieee_address);

                if (_dataSource == 0)
                {
                    _ieee->reject_address();
                    continue;
                }

                _ieee->accept_address();
            }

            filenotfound = 0;
            filename_position = 0;

            if (buscmd == LISTEN)
            {
                init_datasource = false;
            }
        }

        // read single byte from ieee bus
        rdchar = _ieee->get_byte_from_bus();

        if (filenotfound == 1)
        {
            filenotfound = 0;
            _ieee->unlisten();
        }

        // handle bus commands
        if (_ieee->atn_is_low())
        {
            char tmp[8];
            sprintf(tmp, "rd %X\r\n", rdchar);
            _logger->log(tmp);
            if ((rdchar == PET_LOAD_FNAME_ADDR || rdchar == PET_SAVE_FNAME_ADDR))
            {
                getting_filename = 1;

                // clear filename
                memset(progname, 0, 64);
                
                if (rdchar == PET_SAVE_FNAME_ADDR)
                {
                    savefile = 1;
                }
                else
                {
                    savefile = 0;
                }
            }
            else if ((rdchar & PET_OPEN_FNAME_MASK) == PET_OPEN_FNAME_MASK) // open command to another address
            {
                _openFileAddress = (rdchar & PET_OPEN_FNAME_MASK);
            }
            else if (rdchar == PET_READ_CMD_ADDR)
            {


                if (progname[0] == '$')
                {
                    // copy the directory header
                    pgm_memcpy((unsigned char *)_buffer, (unsigned char *)_dirHeader, 7);
                    
                    // print directory title
                    pgm_memcpy((unsigned char *)&_buffer[7], (unsigned char *)_versionString, 24);
                    _buffer[31] = 0x00;
                }
            }
        }
        else
        {
            if (getting_filename == 1) // reading bytes of a filename
            {
                // add character to filename
                progname[filename_position++] = rdchar;
                progname[filename_position] = 0;

                // is this the last character?
                if (_ieee->eoi_is_low())
                {
                    if (progname[0] == '$') // directory request
                    {
                        getting_filename = 0;
                        filename_position = 0;

                        if (!_dataSource->isInitialized())
                        {
                            filenotfound = 1;
                        }
                    }
                    else // file load command
                    {
                        _logger->log("got filename:\r\n");
                        _logger->log((char*)progname);
                        _logger->log("\r\n");


                        filename_position = processFilename(progname, filename_position);

                        // copy the PRG file extension onto the end of the file name
                        pgm_memcpy(&progname[filename_position], (unsigned char *)_fileExtension, 5);

                        // have the full filename now
                        getting_filename = 0;
                        _logger->log((char*)progname);
                        _logger->log((char*)"\r\n");
                        filename_position = 0;
                        gotname = 1;
                    }
                }
            }
        }

        // signal that we are done handling this byte
        _ieee->acknowledge_bus_byte();

        if (init_datasource == false)
        {
            _dataSource->init();
            init_datasource = true;
        }

        if (gotname == 1)
        {
            if (savefile == 1)
            {
                _dataSource->openFileForWriting(progname);
            }
            else
            {
                // file read, either LOAD or OPEN command
                if (!_dataSource->isInitialized() || !_dataSource->openFileForReading(progname))
                {
                    _logger->log("not found\r\n");
                    filenotfound = 1;
                }
                else
                {
                    _logger->log("found file\r\n");
                }
            }
        }
        gotname = 0;

        if ((rdchar == 0x3F) || (rdchar == 0x5F && _ieee->atn_is_low()))
        {
            _logger->log("unlisten\r\n");
            _ieee->signal_ready_for_data();
            _ieee->unlisten();
            // unlistened from the bus, go back to waiting
            continue;
        }
        
        _ieee->signal_ready_for_data();

        // LOAD requested
        if (rdchar == PET_OPEN_IO_ADDR && _ieee->atn_is_low())
        {
            // starting LOAD sequence
            if (filenotfound == 0)
            {
                _ieee->begin_output();

                if (progname[0] == '$') // directory
                {
                    // write directory header
                    _ieee->sendIEEEBytes(_buffer, 32, 0);

                    if (progname[1] == ':')
                    {
                        // change directory command
                        _dataSource->openDirectory((const char*)&progname[2]);
                    }

                    listFiles();
                }
                else // read from file
                {
                    // retrieve full contents of file
                    // and write to ieee bus
                    done_sending = 0;
                    while (done_sending == 0)
                    {
                        bytes_to_send = _dataSource->getNextFileBlock();
                        if (_dataSource->isLastBlock())
                        {
                            //logSerial.transmitString("last block\r\n");
                            done_sending = 1;
                        }

                        _ieee->sendIEEEBytes(_dataSource->getBuffer(), bytes_to_send, done_sending);
                    }
                }
                _ieee->end_output();
            }
        }
        // SAVE requested
        else if (rdchar == PET_SAVE_CMD_ADDR && _ieee->atn_is_low())
        {
            // write file
            // about to write file
            writeFile();
            _ieee->unlisten();
        }
    }
}

unsigned char PETdisk::processFilename(unsigned char* filename, unsigned char length)
{
    unsigned char drive_separator = ':';
    unsigned char* sepptr = (unsigned char*)memmem(filename, length, &drive_separator, 1);
    if (sepptr)
    {
        // found a drive separator
        int seplen = sepptr - filename + 1;
        memmove(filename, sepptr + 1, length - seplen);
        length -= seplen;
    }

    return length;
}

int main(void)
{
    _bufferSize = 0;
    prog_init();
    
    SPI spi;
    spi.init();

    Serial0 serial;
    serial.init(0);  

    Serial1 logSerial;
    logSerial.init(0);

    SerialLogger logger(&logSerial);
    logger.init();

    SD sd(&spi, SPI_CS);
    FAT32 fat32(&sd, _buffer, &_buffer[512], &logSerial);

    checkForFirmware((char*)&_buffer[769], &fat32, &logSerial);

    init_led();
    blink_led(1, 300, 50);

    EspConn espConn(_buffer, &_bufferSize, &serial, &logSerial);
    EspHttp espHttp(&espConn, &logSerial);
    reset_esp();
    for (int i = 0; i < 50; i++)
    {
        _delay_loop_2(65535);
    }

    IEEE488 ieee(&logger);
    ieee.unlisten();

    /*
    unsigned char buscmd;
    unsigned char ieee_address;
    unsigned char rdchar;
    
    unsigned char* progname = (unsigned char*)&_buffer[1024-64];
    int filename_position = 0;
    int filenotfound = 0;
    unsigned char getting_filename = 0;
    unsigned char savefile = 0;
    unsigned char gotname = 0;
    unsigned char done_sending = 0;
    bool init_datasource = false;
    int bytes_to_send = 0;
    unsigned char _openFileAddress = 0;
    */

    NetworkDataSource nds0(&espHttp, _buffer, &_bufferSize, &logSerial);
    NetworkDataSource nds1(&espHttp, _buffer, &_bufferSize, &logSerial);
    NetworkDataSource nds2(&espHttp, _buffer, &_bufferSize, &logSerial);
    NetworkDataSource nds3(&espHttp, _buffer, &_bufferSize, &logSerial);

    NetworkDataSource* nds_array[4];
    nds_array[0] = &nds0;
    nds_array[1] = &nds1;
    nds_array[2] = &nds2;
    nds_array[3] = &nds3;

    PETdisk petdisk;
    petdisk.init(
        &fat32, 
        &logSerial, 
        _buffer, 
        &_bufferSize, 
        &espConn, 
        &espHttp, 
        (NetworkDataSource**)nds_array,
        &ieee,
        &logger);
    
    // execute run loop
    petdisk.run();

    /*
    // main loop
    while(1) 
    {
        if (IEEE_CTL == 0x00) // unlistened
        {
            // not currently listening on the bus.
            // waiting for a valid device address

            dataSource = 0;
            while (dataSource == 0)
            {
                ieee_address = ieee.get_device_address(&buscmd);
                dataSource = petdisk.getDataSource(ieee_address);

                if (dataSource == 0)
                {
                    ieee.reject_address();
                    continue;
                }

                ieee.accept_address();
            }

            filenotfound = 0;
            filename_position = 0;

            if (buscmd == LISTEN)
            {
                init_datasource = false;
            }
        }

        // read single byte from ieee bus
        rdchar = ieee.get_byte_from_bus();

        if (filenotfound == 1)
        {
            filenotfound = 0;
            ieee.unlisten();
        }

        // handle bus commands
        if (ieee.atn_is_low())
        {
            char tmp[8];
            sprintf(tmp, "rd %X\r\n", rdchar);
            logger.log(tmp);
            if ((rdchar == PET_LOAD_FNAME_ADDR || rdchar == PET_SAVE_FNAME_ADDR))
            {
                getting_filename = 1;

                // clear filename
                memset(progname, 0, 64);
                
                if (rdchar == PET_SAVE_FNAME_ADDR)
                {
                    savefile = 1;
                }
                else
                {
                    savefile = 0;
                }
            }
            else if ((rdchar & PET_OPEN_FNAME_MASK) == PET_OPEN_FNAME_MASK) // open command to another address
            {
                _openFileAddress = (rdchar & PET_OPEN_FNAME_MASK);
            }
            else if (rdchar == PET_READ_CMD_ADDR)
            {
                if (progname[0] == '$')
                {
                    // copy the directory header
                    pgm_memcpy((unsigned char *)_buffer, (unsigned char *)_dirHeader, 7);
                    
                    // print directory title
                    pgm_memcpy((unsigned char *)&_buffer[7], (unsigned char *)_versionString, 24);
                    _buffer[31] = 0x00;
                }
            }
        }
        else
        {
            if (getting_filename == 1) // reading bytes of a filename
            {
                // add character to filename
                progname[filename_position++] = rdchar;
                progname[filename_position] = 0;

                // is this the last character?
                if (ieee.eoi_is_low())
                {
                    if (progname[0] == '$') // directory request
                    {
                        getting_filename = 0;
                        filename_position = 0;

                        if (!dataSource->isInitialized())
                        {
                            filenotfound = 1;
                        }
                    }
                    else // file load command
                    {
                        logSerial.transmitString("got filename:\r\n");
                        logSerial.transmitString((char*)progname);
                        logSerial.transmitString("\r\n");


                        filename_position = processFilename(progname, filename_position);

                        // copy the PRG file extension onto the end of the file name
                        pgm_memcpy(&progname[filename_position], (unsigned char *)_fileExtension, 5);

                        // have the full filename now
                        getting_filename = 0;
                        logSerial.transmitString((char*)progname);
                        logSerial.transmitString((char*)"\r\n");
                        filename_position = 0;
                        gotname = 1;
                    }
                }
            }
        }

        // signal that we are done handling this byte
        ieee.acknowledge_bus_byte();

        if (init_datasource == false)
        {
            dataSource->init();
            init_datasource = true;
        }

        if (gotname == 1)
        {
            if (savefile == 1)
            {
                dataSource->openFileForWriting(progname);
            }
            else
            {
                if (!dataSource->isInitialized() || !dataSource->openFileForReading(progname))
                {
                    logger.log("not found\r\n");
                    filenotfound = 1;
                }
                else
                {
                    logger.log("found file\r\n");
                }
            }
        }
        gotname = 0;

        if ((rdchar == 0x3F) || (rdchar == 0x5F && ieee.atn_is_low()))
        {
            logger.log("unlisten\r\n");
            ieee.signal_ready_for_data();
            ieee.unlisten();
            // unlistened from the bus, go back to waiting
            continue;
        }
        
        ieee.signal_ready_for_data();

        // LOAD requested
        if (rdchar == PET_OPEN_IO_ADDR && ieee.atn_is_low())
        {
            // starting LOAD sequence
            if (filenotfound == 0)
            {
                ieee.begin_output();

                if (progname[0] == '$') // directory
                {
                    // write directory header
                    ieee.sendIEEEBytes(_buffer, 32, 0);

                    if (progname[1] == ':')
                    {
                        // change directory command
                        dataSource->openDirectory((const char*)&progname[2]);
                    }

                    listFiles(&ieee, dataSource, &logSerial);
                }
                else // read from file
                {
                    // retrieve full contents of file
                    // and write to ieee bus
                    done_sending = 0;
                    while (done_sending == 0)
                    {
                        bytes_to_send = dataSource->getNextFileBlock();
                        if (dataSource->isLastBlock())
                        {
                            //logSerial.transmitString("last block\r\n");
                            done_sending = 1;
                        }

                        ieee.sendIEEEBytes(dataSource->getBuffer(), bytes_to_send, done_sending);
                    }
                }
                ieee.end_output();
            }
        }
        // SAVE requested
        else if (rdchar == PET_SAVE_CMD_ADDR && ieee.atn_is_low())
        {
            // write file
            // about to write file
            writeFile(&ieee, dataSource);
            ieee.unlisten();
        }

    }
    */

    while(1) {}
    return 0;
}

// can you make the interrupt handler self contained?


// interrupt handler - should read bytes into a buffer.
// this can be read outside the handler to check for end tokens
// would need to be consumed fast enough to prevent buffer overrun
// length can be an atomic (8-bit) value
// you are guaranteed to be able to read this amount of data
// if you allocate enough space to handle any potential reads, you 
// don't even need a ring buffer, just a regular buffer


// turn on serial interrupts to start reading
// turn off when done

ISR(USART0_RX_vect)
{
    // insert character into buffer
    // loop to beginning if needed
    unsigned char a = UDR0;
    _buffer[_bufferSize] = a;
    _bufferSize++;
}