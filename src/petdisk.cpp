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

void writeFile(IEEE488* ieee, DataSource* dataSource)
{
    unsigned int numBytes;
    unsigned char rdchar;
    unsigned char* dataBuffer = dataSource->getBuffer();
    unsigned int writeBufferSize = dataSource->writeBufferSize();
    
    numBytes = 0;
    do
    {
        rdchar = ieee->get_byte_from_bus();
        dataBuffer[numBytes++] = rdchar;
        
        if (numBytes >= writeBufferSize)
        {
            dataSource->writeBufferToFile(numBytes);
            numBytes = 0;
        }
        
        ieee->acknowledge_bus_byte();
        ieee->signal_ready_for_data();
    }
    while (!ieee->eoi_is_low());
    
    if (numBytes > 0)
    {
        dataSource->writeBufferToFile(numBytes);
    }
    
    dataSource->closeFile();
}

void listFiles(IEEE488* ieee, DataSource* dataSource, Serial1* log)
{
    bool gotDir;
    unsigned char startline;
    unsigned int dir_start;
    unsigned char entry[32];
    dir_start = 0x041f;
    unsigned int file = 0;

    log->transmitStringF(PSTR("open dir\r\n"));
    dataSource->openCurrentDirectory();

    do
    {
        // get next directory entry
        gotDir = dataSource->getNextDirectoryEntry();
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
            ieee->sendIEEEBytes(entry, 32, 0);

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
            
            ieee->sendIEEEBytes(entry, 32, 1);
            return;
        }
        else
        {
            if (!dataSource->isHidden() && !dataSource->isVolumeId())
            {
                // check if this is a file that can be used by petdisk
                // currently .PRG files. Soon .SEQ and .REL

                int fname_length = 0;
                unsigned char* fileName = dataSource->getFilename();
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

                if (dataSource->isDirectory())
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

                ieee->sendIEEEBytes(entry, 32, 0);
            }
        }
    }
    while (gotDir == true);
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

    void init(FAT32* fat32, Serial1* log, uint8_t* buffer, uint16_t* bufferSize, EspConn* espConn, EspHttp* espHttp, NetworkDataSource** nds_array);
    void setDataSource(unsigned char id, DataSource* dataSource);
    DataSource* getDataSource(unsigned char id);

    bool readConfigFile(FAT32* fat32, Serial1* log, uint8_t* buffer);
    void printConfig(struct pd_config* pdcfg, Serial1* log);

private:
    DataSource* _dataSources[8];
    EspConn* _espConn;
    EspHttp* _espHttp;

    bool configChanged(struct pd_config* pdcfg);
};

void PETdisk::init(FAT32* fat32, Serial1* log, uint8_t* buffer, uint16_t* bufferSize, EspConn* espConn, EspHttp* espHttp, NetworkDataSource** nds_array)
{
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

int main(void)
{
    _bufferSize = 0;
    prog_init();
    
    SPI spi;
    spi.init();

    Serial serial;
    serial.init(0);  

    Serial1 logSerial;
    logSerial.init(0);

    SerialLogger logger(&serial);
    logger.init();

    //logSerial.transmitString("R\r\n");

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

    DataSource* dataSource = 0;

#ifdef SD_TEST

    sd_test(dataSource, &logSerial);
    while (1) 
    {
        logSerial.transmitStringF(PSTR("*\r\n"));
        _delay_ms(1000);
    }

#endif

    IEEE488 ieee(&logger);
    ieee.unlisten();

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
    petdisk.init(&fat32, &logSerial, _buffer, &_bufferSize, &espConn, &espHttp, (NetworkDataSource**)nds_array);
    //logSerial.transmitString("C\r\n");

    // test
    /*
    dataSource = petdisk.getDataSource(10);
    dataSource->openCurrentDirectory();
    dataSource->getNextDirectoryEntry();

    while (1) {
        
    }
    */

    // test2
    /*
    dataSource = petdisk.getDataSource(10);
    sprintf_P((char*)progname, PSTR("ALIENS.PRG"));
    dataSource->openFileForReading(progname);
    dataSource->getNextFileBlock();
    dataSource->getNextFileBlock();
    dataSource->getNextFileBlock();

    while (1) {}
    */

    // main loop
    while(1) 
    {
        if (IEEE_CTL == 0x00) // unlistened
        {
            //buscmd = ieee.wait_for_device_address(0x08);
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
        else if ((rdchar == 0xF0 || rdchar == 0xF1) && ieee.atn_is_low())
        {
            logger.log("getting filename\r\n");
            getting_filename = 1;

            memset(progname, 0, 255);
            
            if (rdchar == 0xF1)
            {
                savefile = 1;
            }
            else
            {
                savefile = 0;
            }
        }
        else if (getting_filename == 1) // reading bytes of a filename
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
                    // copy the PRG file extension onto the end of the file name
                    pgm_memcpy(&progname[filename_position], (unsigned char *)_fileExtension, 5);

                    // have the full filename now
                    getting_filename = 0;
                    logger.log((char*)progname);
                    logger.log("**\r\n");
                    filename_position = 0;
                    gotname = 1;
                }
            }
        }
        else if (rdchar == 0x60 && ieee.atn_is_low())
        {
            logger.log("checking for dir\r\n");
            if (progname[0] == '$')
            {
                // copy the directory header
                pgm_memcpy((unsigned char *)_buffer, (unsigned char *)_dirHeader, 7);
                
                // print directory title
                pgm_memcpy((unsigned char *)&_buffer[7], (unsigned char *)_versionString, 24);
                _buffer[31] = 0x00;
            }
        }

        // signal that we are done handling this byte
        ieee.acknowledge_bus_byte();

        if (init_datasource == false)
        {
            /*
            if (!dataSource->init())
            {
                break;
            }
            */
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

            //memset(progname, 0, 255);
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
        if (rdchar == 0x60 && ieee.atn_is_low())
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
        else if (rdchar == 0x61 && ieee.atn_is_low())
        {
            // write file
            // about to write file
            writeFile(&ieee, dataSource);
            ieee.unlisten();
        }

    }

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