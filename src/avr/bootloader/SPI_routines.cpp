/*
    SPI_routines.c
    SPI Routines in the PETdisk storage device
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
#include "SPI_routines.h"

void SPI::init()
{
    SPI_PORT = 0x00;
    SPI_PORT |= 1 << SPI_CS;
    SPI_PORT |= 1 << SPI_CS2;
    SPI_CONTROL |= 1 << SPI_CS;
    SPI_CONTROL |= 1 << SPI_CS2;
    SPI_CONTROL |= 1 << SPI_MOSI;
    SPI_CONTROL |= 1 << SPI_SCK; 

    SPCR = 0x52;
    SPSR = 0x00;
}

uint8_t SPI::transmit(uint8_t data)
{
    SPDR = data;
    while ( !(SPSR & (1<<SPIF)) ) {}

    data = SPDR;
    return data;
}

uint8_t SPI::receive()
{
    return transmit(0xff);
}
