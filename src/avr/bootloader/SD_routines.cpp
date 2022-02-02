/*
    SD_routines.c
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
#include <avr/io.h>
#include <avr/pgmspace.h>
#include "SPI_routines.h"
#include "SD_routines.h"
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <stdio.h>

void SD::cs_select()
{
    SPI_PORT &= ~(1 << _cs);
}

void SD::cs_unselect()
{
    SPI_PORT |= (1 << _cs);
}

//******************************************************************
//Function  : to initialize the SD/SDHC card in SPI mode
//Arguments : none
//return    : unsigned char; will be 0 if no error,
//            otherwise the response byte will be sent
//******************************************************************
unsigned char SD::init()
{
    unsigned char i, response, SD_version;
    unsigned int retry = 0 ;
    
    cs_unselect();

    for(i = 0; i < 10; i++)
    {
        _spi->transmit(0xff);   //80 clock pulses spent before sending the first command
    }

    cs_select();

    do
    {
       response = sendCommand(GO_IDLE_STATE, 0); //send 'reset & go idle' command
       retry++;
       if(retry>0x20)
       {
          return 1;   //time out, card not detected
       }
       
    } while(response != 0x01);

    cs_unselect();
    _spi->transmit (0xff);
    _spi->transmit (0xff);

    retry = 0;
    SD_version = 2; //default set to SD compliance with ver2.x; 
                    //this may change after checking the next command
    do
    {
        response = sendCommand(SEND_IF_COND, 0x000001AA); //Check power supply status, mendatory for SDHC card
        
        retry++;
        if(retry > 0xfe) 
        {
            SD_version = 1;
            _cardType = 1;
            return 3;
        } //time out

    } while(response != 0x01);

    retry = 0;

    do
    {
        response = sendCommand(APP_CMD,0); //CMD55, must be sent before sending any ACMD command
        response = sendCommand(SD_SEND_OP_COND,0x40000000); //ACMD41

        if (response != 0x00)
        {
            retry++;
            if (retry > 0xfe)
            {
                return 2;  //time out, card initialization failed
            }
        } 

    } while( response != 0x00 );


    retry = 0;
    _SDHC_flag = 0;

    if (SD_version == 2)
    { 
        do
        {
            response = sendCommand(READ_OCR,0);
            retry++;
            if(retry>0xfe) 
            {
                _cardType = 0;
                break;
            } //time out
        } while(response != 0x00);

        if (_SDHC_flag == 1)
        {
            _cardType = 2;
        }
        else
        {
            _cardType = 3;
        }
    }

    return 0; //successful return
}

//******************************************************************
//Function  : to send a command to SD card
//Arguments : unsigned char (8-bit command value)
//            & unsigned long (32-bit command argument)
//return    : unsigned char; response byte
//******************************************************************
unsigned char SD::sendCommand(unsigned char cmd, unsigned long arg)
{
    unsigned char response, retry=0, status;

    //SD card accepts byte address while SDHC accepts block address in multiples of 512
    //so, if it's SD card we need to convert block address into corresponding byte address by 
    //multipying it with 512. which is equivalent to shifting it left 9 times
    //following 'if' loop does that

    if(_SDHC_flag == 0)
    {
        if(cmd == READ_SINGLE_BLOCK     ||
           cmd == READ_MULTIPLE_BLOCKS  ||
           cmd == WRITE_SINGLE_BLOCK    ||
           cmd == WRITE_MULTIPLE_BLOCKS ||
           cmd == ERASE_BLOCK_START_ADDR|| 
           cmd == ERASE_BLOCK_END_ADDR ) 
        {
            arg = arg << 9;
        }       
    }

    cs_select();

    _spi->transmit(cmd | 0x40); //send command, first two bits always '01'
    _spi->transmit(arg>>24);
    _spi->transmit(arg>>16);
    _spi->transmit(arg>>8);
    _spi->transmit(arg);

    if (cmd == SEND_IF_COND)     //it is compulsory to send correct CRC for CMD8 (CRC=0x87) & CMD0 (CRC=0x95)
    {
        _spi->transmit(0x87);    //for remaining commands, CRC is ignored in SPI mode
    }
    else if (cmd == 55)
    { 
        _spi->transmit(0x65);
    }
    else if (cmd == 41)
    {
        _spi->transmit(0x77);
    }
    else
    {
      _spi->transmit(0x95); 
    }

    while((response = _spi->receive()) == 0xff) //wait response
    {
       if(retry++ > 0xfe) break; //time out error
    }

    if(response == 0x00 && cmd == 58)  //checking response of CMD58
    {
        status = _spi->receive() & 0x40;     //first byte of the OCR register (bit 31:24)
        if(status == 0x40)
        { 
            _SDHC_flag = 1;  //we need it to verify SDHC card
        }
        else 
        {
            _SDHC_flag = 0;
        }

        _spi->receive(); //remaining 3 bytes of the OCR register are ignored here
        _spi->receive(); //one can use these bytes to check power supply limits of SD
        _spi->receive(); 
    } 
    else if (cmd == 8)
    {
        // receive 4 bytes
        // TODO: check bytes for voltage range
        _spi->receive(); 
        _spi->receive(); 
        _spi->receive(); 
        _spi->receive(); 
    }

    _spi->receive(); //extra 8 CLK
    cs_unselect();

    return response; //return state
}

//******************************************************************
//Function  : to read a single block from SD card
//Arguments : none
//return    : unsigned char; will be 0 if no error,
//            otherwise the response byte will be sent
//******************************************************************
unsigned char SD::readSingleBlock(unsigned long startBlock, unsigned char* buffer)
{
    unsigned char response;
    unsigned int i, retry=0;

    response = sendCommand(READ_SINGLE_BLOCK, startBlock); //read a Block command

    if (response != 0x00)
    {
        return response; //check for SD status: 0x00 - OK (No flags set)
    }

    cs_select();

    retry = 0;
    while (_spi->receive() != 0xfe) //wait for start block token 0xfe (0x11111110)
    {
        if (retry++ > 0xfffe)
        {
            cs_unselect(); 
            return 1; //return if time-out
        } 
    }

    for ( i = 0; i < 512; i++) //read 512 bytes
    {
        buffer[i] = _spi->receive();
    }

    _spi->receive(); //receive incoming CRC (16-bit), CRC is ignored here
    _spi->receive();

    _spi->receive(); //extra 8 clock pulses
    cs_unselect();

    return 0;
}

//******************************************************************
//Function  : to write to a single block of SD card
//Arguments : none
//return    : unsigned char; will be 0 if no error,
//            otherwise the response byte will be sent
//******************************************************************
unsigned char SD::writeSingleBlock(unsigned long startBlock, unsigned char* buffer)
{
    unsigned char response;
    unsigned int i, retry=0;

    response = sendCommand(WRITE_SINGLE_BLOCK, startBlock); //write a Block command
  
    if (response != 0x00)
    {
        return response; //check for SD status: 0x00 - OK (No flags set)
    }

    cs_select();

    _spi->transmit(0xfe);     //Send start block token 0xfe (0x11111110)

    for (i = 0; i < 512; i++)    //send 512 bytes data
    {
        _spi->transmit(buffer[i]);
    }

    _spi->transmit(0xff);     //transmit dummy CRC (16-bit), CRC is ignored here
    _spi->transmit(0xff);

    response = _spi->receive();

    if ( (response & 0x1f) != 0x05) //response= 0xXXX0AAA1 ; AAA='010' - data accepted
    {                              //AAA='101'-data rejected due to CRC error
        cs_unselect();              //AAA='110'-data rejected due to write error
        return response;
    }

    while (!_spi->receive()) //wait for SD card to complete writing and get idle
    {
        if(retry++ > 0xfffe)
        {
            cs_unselect();
            return 1;
        }
    }

    cs_unselect();
    _spi->transmit(0xff);   //just spend 8 clock cycle delay before reasserting the CS line
    cs_select();         //re-asserting the CS line to verify if card is still busy

    while (!_spi->receive()) //wait for SD card to complete writing and get idle
    {
        if(retry++ > 0xfffe)
        {
            cs_unselect();
            return 1;
        }
    }
    cs_unselect();

    return 0;
}
