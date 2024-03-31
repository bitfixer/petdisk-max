/*
    SPI_routines.h
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
#ifndef _SPI_ROUTINES_H_
#define _SPI_ROUTINES_H_

#include <avr/io.h>
#include <stdint.h>

#define SPI_CONTROL     DDRB
#define SPI_PORT        PORTB
#define SPI_CS          PB4
#define SPI_CS2         PB3
#define SPI_MOSI        PB5
#define SPI_SCK         PB7

class SPI {
public:
    SPI() {}
    ~SPI() {}

    void init();
    uint8_t transmit(uint8_t data);
    uint8_t receive();
};

#endif
