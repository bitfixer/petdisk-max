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

// ESP32 implementation of IEEE

#include "IEEE488.h"
#include <stdio.h>
#include <string.h>
#include "IEEE488_hardware.h"

namespace bitfixer {

void IEEE488::initWithLogger(SerialLogger* logger)
{
    _logger = logger;

    set_datadir_output();
    set_atn_input();
    set_data_input();
}

void IEEE488::wait_for_dav_high()
{
   while (read_dav() == 0) {}
}

void IEEE488::wait_for_dav_low()
{
   while (read_dav() != 0) {}
}

void IEEE488::wait_for_atn_high()
{
   while (read_atn() == 0) {}
}

bool IEEE488::is_atn_asserted()
{
   if (read_atn() == 0)
   {
       return true;
   }

   return false;
}

void IEEE488::wait_for_atn_low()
{
   while (read_atn() != 0) {}
}

void IEEE488::wait_for_nrfd_high()
{
   while (read_nrfd() == 0) {}
}

void IEEE488::wait_for_nrfd_low()
{
    while (read_nrfd() != 0) {}
}

void IEEE488::wait_for_ndac_high()
{
   while (read_ndac() == 0) {}
}

void IEEE488::wait_for_ndac_low()
{
   while (read_ndac() != 0) {}
}

uint8_t IEEE488::wait_for_ndac_low_or_atn_low()
{
    while (1) 
    {
        if (read_atn() == 0)
        {
            return IEEEBusSignal::ATN;
        }
        else if (read_ndac() == 0)
        {
            return IEEEBusSignal::NDAC;
        }
    }
}

IEEEBusSignal IEEE488::wait_for_ndac_high_or_atn_low()
{
    while (1)
    {
        if (read_atn() == 0)
        {
            return IEEEBusSignal::ATN;
        }
        else if (read_ndac() != 0)
        {
            return IEEEBusSignal::NDAC;
        }
    }
}

uint8_t IEEE488::wait_for_nrfd_high_or_atn_low()
{
   while (1)
   {
       if (read_atn() == 0)
       {
           return IEEEBusSignal::ATN;
       }
       else if (read_nrfd() != 0)
       {
           return IEEEBusSignal::NRFD;
       }
   }
}

void IEEE488::signal_ready_for_data()
{
    lower_ndac();
    raise_nrfd();
    //IEEE_PORT = NOT_NDAC_MASK;
}

void IEEE488::recv_byte(uint8_t *byte)
{
    //*byte = ~(DATA_PIN);
    uint8_t recvByte = 0;
    ieee_read_data_byte(recvByte);

    //_logger->printf("recv %d %d %d %d %d %d %d %d\n", bits[7], bits[6], bits[5], bits[4], bits[3], bits[2], bits[1], bits[0]);
    *byte = ~recvByte;
}

void IEEE488::write_byte_to_data_bus(uint8_t byte)
{
     // put byte on data lines
    byte = ~byte;
    ieee_write_data_byte(byte);
}

void IEEE488::send_byte(uint8_t byte, int last)
{
    write_byte_to_data_bus(byte);
    wait_for_nrfd_high();

    if (last != 0)
    {
        lower_eoi();
    }
    lower_dav();

    // wait for NDAC high
    wait_for_ndac_high();

    // raise DAV and EOI
    raise_dav_and_eoi();
}

void IEEE488::raise_dav_and_eoi()
{
    //IEEE_PORT = DAV_MASK | EOI_MASK;
    raise_dav();
    raise_eoi();
}

void IEEE488::unlisten()
{
    set_data_input();

    // all bus lines input
    set_atn_input();
    set_ndac_input();
    set_dav_input();
    set_nrfd_input();
    set_eoi_input();

    _unlistened = true;
}

bool IEEE488::is_unlistened()
{
    return _unlistened;
}

/*
unsigned char IEEE488::get_device_address(unsigned char* dir, bool* success)
{
    unsigned char primary_address;
    // wait for atn signal
    wait_for_atn_low();

    set_ndac_output();
    lower_ndac();

    // wait for primary addresss
    wait_for_dav_low();

    // read data
    recv_byte(&primary_address);

    set_nrfd_output();
    lower_nrfd();
    raise_ndac();
    
    *dir = primary_address & 0xF0;
    primary_address = primary_address & 0x0F;
    
    *success = true;
    return primary_address;
}
*/

uint8_t IEEE488::get_device_address(uint8_t* dir, bool* success)
{
    *success = false;
    uint8_t primary_address;
    // wait for atn signal
    

    // NOTE: avr misses atn signals if we return here without waiting for it
    /*
    if (!is_atn_asserted())
    {
        // no ATN signal, return
        return 0;
    }
    */
    
    wait_for_atn_low();
    
    // lower NDAC to respond
    set_ndac_output();
    lower_ndac();

    // wait for primary addresss
    wait_for_dav_low();

    // read data
    recv_byte(&primary_address);
    
    // set NRFD to output
    set_nrfd_output();
    // lower NRFD, raise ndac

    lower_nrfd();
    raise_ndac();
    
    *dir = primary_address & 0xF0;
    primary_address = primary_address & 0x0F;
    
    *success = true;
    return primary_address;
}

void IEEE488::accept_address()
{
    // raise NDAC
    //IEEE_PORT = NOT_NRFD_MASK;
    raise_ndac();
    // wait for data to finish
    wait_for_dav_high();
    
    // release control and handle transaction
    //IEEE_PORT = NOT_NDAC_MASK;
    raise_nrfd();
    lower_ndac();

    _unlistened = false;
}

void IEEE488::reject_address()
{
    // unlisten the bus
    unlisten();
    // wait for atn to release
    wait_for_atn_high();
}

void IEEE488::set_data_output()
{
    // set all data lines to output
    ieee_set_data_output();
}

void IEEE488::set_data_input()
{
    ieee_set_data_input();
}

// configure IEEE bus to begin sending bytes
void IEEE488::begin_output()
{
    set_nrfd_input();
    wait_for_atn_high();

    set_ndac_input();
    set_dav_output();
    set_eoi_output();

    raise_dav();
    raise_eoi();

    set_data_output();

    wait_for_ndac_low();
}

void IEEE488::end_output()
{
    set_ndac_output();
    lower_ndac();

    set_dav_input();
    set_eoi_input();

    set_nrfd_output();
    raise_nrfd();

    set_data_input();
}

uint8_t IEEE488::sendIEEEByteCheckForATN(uint8_t byte)
{
    uint8_t result = 0;
    // put the byte on the data lines
    write_byte_to_data_bus(byte);
    
    result = wait_for_ndac_low_or_atn_low();
    if (result == ATN)
    {
        return result;
    }

    // wait for NRFD high
    result = wait_for_nrfd_high_or_atn_low();
    if (result == ATN)
    {
        return result;
    }

    lower_dav();
    return 0;
}

uint8_t IEEE488::sendIEEEByteCheckForATN2(uint8_t byte, bool last)
{
    uint8_t result = 0;
    
    result = wait_for_ndac_low_or_atn_low();
    if (result == ATN)
    {
        return result;
    }

    // wait for NRFD high
    result = wait_for_nrfd_high_or_atn_low();
    if (result == ATN)
    {
        return result;
    }

    write_byte_to_data_bus(byte);
    if (last == true)
    {
        lower_dav();
        lower_eoi();
    }
    else
    {
        lower_dav();
    }
    return 0;
}

void IEEE488::sendIEEEBytes(uint8_t *entry, int size, uint8_t isLast)
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
    return _atn_low;
}

bool IEEE488::eoi_is_low()
{
    return _eoi_low;
}

uint8_t IEEE488::get_byte_from_bus()
{
    uint8_t rdchar;
    wait_for_dav_low();
    // lower NDAC and NRFD
    lower_ndac();
    lower_nrfd();

    recv_byte(&rdchar);
    uint8_t atnval = read_atn();
    uint8_t eoival = read_eoi();

    _atn_low = (atnval == 0) ? true : false;
    _eoi_low = (eoival == 0) ? true : false;

    return rdchar;
}

void IEEE488::acknowledge_bus_byte()
{
    lower_nrfd();
    raise_ndac();
    wait_for_dav_high();
}

}