/*
    sd_routines.h
    SD Routines in the PETdisk storage device
    Copyright (C) 2011 Michael Hill

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    
    Contact the author at bitfixer@bitfixer.com
    http://bitfixer.com
    
*/
#ifndef _SD_ROUTINES_H_
#define _SD_ROUTINES_H_

#include <stddef.h>
#include "SPI_routines.h"

//Use following macro if you don't want to activate the multiple block access functions
//those functions are not required for FAT32

#define FAT_TESTING_ONLY         

//SD commands, many of these are not used here
#define GO_IDLE_STATE            0
#define SEND_OP_COND             1
#define SEND_IF_COND			 8
#define SEND_CSD                 9
#define STOP_TRANSMISSION        12
#define SEND_STATUS              13
#define SET_BLOCK_LEN            16
#define READ_SINGLE_BLOCK        17
#define READ_MULTIPLE_BLOCKS     18
#define WRITE_SINGLE_BLOCK       24
#define WRITE_MULTIPLE_BLOCKS    25
#define ERASE_BLOCK_START_ADDR   32
#define ERASE_BLOCK_END_ADDR     33
#define ERASE_SELECTED_BLOCKS    38
#define SD_SEND_OP_COND			 41   //ACMD
#define APP_CMD					 55
#define READ_OCR				 58
#define CRC_ON_OFF               59

#define ON     1
#define OFF    0

class SD 
{
public:
    SD()
    : _spi(NULL)
    , _startBlock(0)
    , _totalBlocks(0)
    , _SDHC_flag(0)
    , _cardType(0)
    , _cs(0)
    {

    }

    SD(bSPI* spi, int cs)
    : _spi(spi)
    , _startBlock(0)
    , _totalBlocks(0)
    , _SDHC_flag(0)
    , _cardType(0)
    , _cs(cs)
    {
    }

    ~SD()
    {
        _spi = 0;
    }

    uint8_t init();
    uint8_t initWithSPI(bSPI* spi, int cs);
    uint8_t sendCommand(uint8_t cmd, uint32_t arg);
    uint8_t readSingleBlock(uint32_t startBlock, uint8_t* buffer);
    uint8_t writeSingleBlock(uint32_t startBlock, uint8_t* buffer);

private:
    bSPI* _spi;
    uint32_t _startBlock;
    uint32_t _totalBlocks;
    uint8_t _SDHC_flag;
    uint8_t _cardType;
    int _cs;

    void cs_select();
    void cs_unselect();

};

#endif
