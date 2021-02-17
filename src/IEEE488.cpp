/*
    IEEE488.c
    IEEE Routines in the PETdisk storage device
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


#include "IEEE488.h"

#include <avr/io.h>
#include <stdio.h>
#include <string.h>

void IEEE488::wait_for_dav_high()
{
    unsigned char rdchar;
    rdchar = IEEE_PIN;
    rdchar = rdchar & DAV_MASK;
    while (rdchar == 0x00)
    {
        rdchar = IEEE_PIN;
        rdchar = rdchar & DAV_MASK;
    }
}

void IEEE488::wait_for_dav_low()
{
    unsigned char rdchar;
    rdchar = IEEE_PIN;
    rdchar = rdchar & DAV_MASK;
    while (rdchar != 0x00)
    {
        rdchar = IEEE_PIN;
        rdchar = rdchar & DAV_MASK;
    }
}

void IEEE488::wait_for_atn_high()
{
    unsigned char rdchar;
    rdchar = IEEE_PIN;
    rdchar = rdchar & ATN_MASK;
    while (rdchar == 0x00)
    {
        rdchar = IEEE_PIN;
        rdchar = rdchar & ATN_MASK;
    }
}

void IEEE488::wait_for_atn_low()
{
    unsigned char rdchar;
    rdchar = IEEE_PIN;
    rdchar = rdchar & ATN_MASK;
    while (rdchar != 0x00)
    {
        rdchar = IEEE_PIN;
        rdchar = rdchar & ATN_MASK;
    }
}

void IEEE488::wait_for_nrfd_high()
{
    unsigned char rdchar;
    rdchar = IEEE_PIN;
    rdchar = rdchar & NRFD_MASK;
    while (rdchar == 0x00)
    {
        rdchar = IEEE_PIN;
        rdchar = rdchar & NRFD_MASK;
    }
}

void IEEE488::wait_for_ndac_high()
{
    unsigned char rdchar;
    rdchar = IEEE_PIN;
    rdchar = rdchar & NDAC_MASK;
    while (rdchar == 0x00)
    {
        rdchar = IEEE_PIN;
        rdchar = rdchar & NDAC_MASK;
    }
}

void IEEE488::wait_for_ndac_low()
{
    unsigned char rdchar;
    rdchar = IEEE_PIN;
    rdchar = rdchar & NDAC_MASK;
    while (rdchar != 0x00)
    {
        rdchar = IEEE_PIN;
        rdchar = rdchar & NDAC_MASK;
    }
}

unsigned char IEEE488::wait_for_ndac_low_or_atn_low()
{
    unsigned char rdchar;
    while (1)
    {
        rdchar = IEEE_PIN;
        if ((rdchar & ATN_MASK) == 0x00)
        {
            return ATN_MASK;
        }
        else if ((rdchar & NDAC_MASK) == 0x00)
        {
            return NDAC_MASK;
        }
    }
}

unsigned char IEEE488::wait_for_ndac_high_or_atn_low()
{
    unsigned char rdchar;
    while (1)
    {
        rdchar = IEEE_PIN;
        if ((rdchar & ATN_MASK) == 0x00)
        {
            return ATN_MASK;
        }
        else if ((rdchar & NDAC_MASK) != 0x00)
        {
            return NDAC_MASK;
        }
    }
}

unsigned char IEEE488::wait_for_nrfd_high_or_atn_low()
{
    unsigned char rdchar;
    while (1)
    {
        rdchar = IEEE_PIN;
        if ((rdchar & ATN_MASK) == 0x00)
        {
            return ATN_MASK;
        }
        else if ((rdchar & NRFD_MASK) != 0x00)
        {
            return NRFD_MASK;
        }
    }
}

void IEEE488::lower_nrfd()
{
    IEEE_PORT = IEEE_PORT & NOT_NRFD_MASK;
}

void IEEE488::raise_nrfd()
{
    IEEE_PORT = IEEE_PORT | NRFD_MASK;
}

void IEEE488::signal_ready_for_data()
{
    //raise_nrfd();
    IEEE_PORT = NOT_NDAC_MASK;
}

void IEEE488::lower_ndac()
{
    IEEE_PORT = IEEE_PORT & NOT_NDAC_MASK;
}

void IEEE488::raise_ndac()
{
    IEEE_PORT = IEEE_PORT | NDAC_MASK;
}

void IEEE488::recv_byte(unsigned char *byte)
{
    *byte = ~(DATA_PIN);
}

void IEEE488::send_byte(unsigned char byte, int last)
{
    unsigned char temp, temp2;
    // put the byte on the data lines
    DATA_PORT = ~byte;
    
    // wait for NRFD high
    wait_for_nrfd_high();
    
    // lower DAV and EOI
    
    if (last == 0)
    {
        //lower DAV
        //temp = DAV_MASK;
        //IEEE_PORT = ~temp;
        lower_dav();
    }
    else 
    {
        temp = DAV_MASK;
        temp2 = EOI_MASK;
        IEEE_PORT = (~temp) & (~temp2);
    }

    // wait for NDAC high
    wait_for_ndac_high();
    
    // raise DAV and EOI
    temp = DAV_MASK | EOI_MASK;
    // output to bus
    IEEE_PORT = temp;
    
}

void IEEE488::raise_dav_and_eoi()
{
    IEEE_PORT = DAV_MASK | EOI_MASK;
}

void IEEE488::unlisten()
{
    // all bus lines input
    IEEE_CTL = 0x00;
    // set pullups
    IEEE_PORT = 0xff;
}

unsigned char IEEE488::get_device_address(unsigned char* dir)
{
    unsigned char primary_address;
    // wait for atn signal
    wait_for_atn_low();
    
    // lower NDAC to respond
    IEEE_PORT = NOT_NDAC_MASK;
    //IEEE_PORT = IEEE_PORT & NOT_NDAC_MASK;
    // set output lines
    IEEE_CTL = IEEE_CTL | NDAC_MASK;
    
    // wait for primary addresss
    wait_for_dav_low();
    
    // set NRFD to output
    IEEE_CTL = IEEE_CTL | NRFD_MASK;
    
    // lower NRFD
    IEEE_PORT = IEEE_PORT & NOT_NRFD_MASK;
    
    // read data
    recv_byte(&primary_address);

    *dir = primary_address & 0xF0;
    primary_address = primary_address & 0x0F;
    
    return primary_address;
}

unsigned char IEEE488::wait_for_device_address(unsigned char my_address)
{
    unsigned char primary_address, dir;
    primary_address = 0;
    dir = 0;
    while (primary_address != my_address)
    {
        primary_address = get_device_address(&dir);
        
        if (primary_address == my_address && (dir == TALK || dir == LISTEN))
        {
            accept_address();
        }
        else 
        {
            reject_address();
        }
    }
    return dir;
}

void IEEE488::accept_address()
{
    // raise NDAC
    IEEE_PORT = NOT_NRFD_MASK;
    // wait for data to finish
    wait_for_dav_high();
    
    // release control and handle transaction
    IEEE_PORT = NOT_NDAC_MASK;
}

void IEEE488::reject_address()
{
    // unlisten the bus
    unlisten();
    // wait for atn to release
    wait_for_atn_high();
}

void IEEE488::set_nrfd_input()
{
    IEEE_CTL = IEEE_CTL & NOT_NRFD_MASK;
}

void IEEE488::set_ndac_input()
{
    IEEE_CTL = IEEE_CTL & NOT_NDAC_MASK;
}

void IEEE488::set_dav_output()
{
    IEEE_CTL = IEEE_CTL | DAV_MASK;
}

void IEEE488::set_eoi_output()
{
    IEEE_CTL = IEEE_CTL | EOI_MASK;
}

void IEEE488::set_data_output()
{
    // set all data lines to output
    DATA_CTL = 0xFF;
}

void IEEE488::set_data_input()
{
    DATA_CTL = 0x00;
}

// configure IEEE bus to begin sending bytes
void IEEE488::begin_output()
{
    set_nrfd_input();
    wait_for_atn_high();

    set_ndac_input();
    set_dav_output();
    set_eoi_output();

    IEEE_PORT = 0xFF;
    set_data_output();

    wait_for_ndac_low();
}

void IEEE488::end_output()
{
    IEEE_PORT = 0xFF;
    IEEE_CTL = NRFD_MASK | NDAC_MASK;

    set_data_input();
    lower_ndac();
    unlisten();
}

unsigned char IEEE488::sendIEEEByteCheckForATN(unsigned char byte)
{
    unsigned char temp, temp2;
    unsigned char result = 0;
    // put the byte on the data lines
    DATA_PORT = ~byte;
    
    result = wait_for_ndac_low_or_atn_low();
    if (result == ATN_MASK)
    {
        return result;
    }

    // wait for NRFD high
    result = wait_for_nrfd_high_or_atn_low();
    if (result == ATN_MASK)
    {
        return result;
    }

    lower_dav();
}

void IEEE488::lower_dav()
{
    unsigned char temp = IEEE_PORT;
    temp &= ~DAV_MASK;
    IEEE_PORT = temp;
}

void IEEE488::raise_dav()
{
    unsigned char temp = IEEE_PORT;
    temp |= DAV_MASK;
    IEEE_PORT = temp;
}

void IEEE488::sendIEEEBytes(unsigned char *entry, int size, unsigned char isLast)
{
    int i;
    int last = size;
    
    if (isLast)
    {
        last--;
    }
    
    for (i = 0; i < last; i++)
    {
        send_byte(entry[i], 0);
    }
    
    if (isLast)
    {
        send_byte(entry[i], 1);
    }
}

bool IEEE488::atn_is_low()
{
    if ((_busval & ATN_MASK) == 0x00)
    {
        return true;
    }

    return false;
}

bool IEEE488::eoi_is_low()
{
    if ((_busval & EOI_MASK) == 0x00)
    {
        return true;
    }

    return false;
}

unsigned char IEEE488::get_byte_from_bus()
{
    unsigned char rdchar;
    wait_for_dav_low();
    // lower NDAC and NRFD
    IEEE_PORT = NOT_NDAC_MASK & NOT_NRFD_MASK;
    recv_byte(&rdchar);
    _busval = IEEE_PIN;
    return rdchar;
}

void IEEE488::acknowledge_bus_byte()
{
    //lower_nrfd();
    IEEE_PORT = NOT_NRFD_MASK;
    wait_for_dav_high();
}