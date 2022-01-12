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
#include "hardware.h"

void bSPI::init()
{
    spi_init();
}

uint8_t bSPI::transmit(uint8_t data)
{
    return spi_transmit(data);
}

uint8_t bSPI::receive()
{
    return transmit(0xff);
}

void bSPI::cs_select()
{
    spi_cs_select();
}

void bSPI::cs_unselect()
{
    spi_cs_unselect();
}
