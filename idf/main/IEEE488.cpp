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
#include "hardware.h"

namespace bitfixer {

static constexpr const int PIN_TIMEOUT_US = 200000;
static IEEE488 _ieee;

void IEEE488::init() {
    set_datadir_output();
    set_atn_input();
    ieee_set_data_input();
}

IEEE488* IEEE488::get_instance() {
    return &_ieee;
}

bool IEEE488::wait_for_dav_high(int timeout_us)
{
    int64_t time_start_us = get_time_us();
    while (read_dav() == 0) {
        if (timeout_us < (get_time_us()-time_start_us)) {
            unlisten();
            return false;
        }
    }
    return true;
}

bool IEEE488::wait_for_dav_low(int timeout_us)
{
    int64_t time_start_us = get_time_us();
    while (read_dav() != 0) {
        if (timeout_us < (get_time_us()-time_start_us)) {
            unlisten();
            return false;
        }
    }
    return true;
}

bool IEEE488::wait_for_atn_high(int timeout_us)
{
    int64_t start_time_us = get_time_us();
    while (read_atn() == 0) {
        if ((get_time_us() - start_time_us) > timeout_us) {
            unlisten();
            return false;
        }
    }
    return true;
}

bool IEEE488::wait_for_nrfd_high()
{
    int64_t start_time_us = get_time_us();
    while (read_nrfd() == 0) {
        if ((get_time_us() - start_time_us) > PIN_TIMEOUT_US) {
            unlisten();
            return false;
        }
    }
    return true;
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
    int64_t start_time_us = get_time_us();
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
        if ((get_time_us() - start_time_us) > PIN_TIMEOUT_US) {
            unlisten();
            return TIMEOUT;
        }
    }
}

IEEEBusSignal IEEE488::wait_for_ndac_high_or_atn_low()
{
    int64_t start_time_us = get_time_us();
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
        if ((get_time_us() - start_time_us) > PIN_TIMEOUT_US) {
            unlisten();
            return TIMEOUT;
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
}

void IEEE488::recv_byte(uint8_t *byte)
{
    uint8_t recvByte = 0;
    ieee_read_data_byte(recvByte);

    *byte = ~recvByte;
}

void IEEE488::write_byte_to_data_bus(uint8_t byte)
{
     // put byte on data lines
    byte = ~byte;
    ieee_write_data_byte(byte);
}

bool IEEE488::send_byte(uint8_t byte, int last)
{
    write_byte_to_data_bus(byte);
    if (!wait_for_nrfd_high()) {
        return false;
    }

    if (last != 0)
    {
        lower_eoi();
    }
    lower_dav();

    // wait for NDAC high
    wait_for_ndac_high();

    // raise DAV and EOI
    raise_dav_and_eoi();
    return true;
}

void IEEE488::raise_dav_and_eoi()
{
    //IEEE_PORT = DAV_MASK | EOI_MASK;
    raise_dav();
    raise_eoi();
}

void IEEE488::unlisten()
{
    ieee_set_data_input();

    // all bus lines input
    set_atn_input();
    set_ndac_input();
    set_dav_input();
    set_nrfd_input();
    set_eoi_input();

    _unlistened = true;
    clear_atn();
}

bool IEEE488::is_unlistened()
{
    return _unlistened;
}

uint8_t IEEE488::get_device_address(uint8_t* dir, bool* success)
{
    *success = false;
    uint8_t primary_address;
    // wait for atn signal
    wait_atn_isr();
    
    // lower NDAC to respond
    if (!wait_for_dav_low(PIN_TIMEOUT_US)) {
        return 0xFF;
    }

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
    raise_ndac();
    // wait for data to finish
    wait_for_dav_high(PIN_TIMEOUT_US);
    
    // release control and handle transaction
    raise_nrfd();
    lower_ndac();

    _unlistened = false;
}

void IEEE488::reject_address()
{
    // unlisten the bus
    unlisten();
    // wait for atn to release
    wait_for_atn_high(PIN_TIMEOUT_US);
}

bool IEEE488::begin_output_start()
{
    set_nrfd_input();
    if (!wait_for_atn_high(PIN_TIMEOUT_US)) {
        return false;
    }

    set_ndac_input();
    set_dav_output();
    set_eoi_output();

    raise_dav();
    raise_eoi();

    ieee_set_data_output();

    wait_for_ndac_low();
    return true;
}

void IEEE488::end_output()
{
    set_ndac_output();
    lower_ndac();

    set_dav_input();
    set_eoi_input();

    set_nrfd_output();
    raise_nrfd();

    ieee_set_data_input();
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

bool IEEE488::sendIEEEBytes(uint8_t *entry, int size, uint8_t isLast)
{
    int i;
    int last = size;
    
    if (isLast)
    {
        last--;
    }
    
    for (i = 0; i < last; i++)
    {
        if (!send_byte(entry[i], 0)) {
            return false;
        }
    }
    
    if (isLast)
    {
        if (!send_byte(entry[i], 1)) {
            return false;
        }
    }
    return true;
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
    if (!wait_for_dav_low(PIN_TIMEOUT_US)) {
        return 0xFF;
    }
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
    if (!wait_for_dav_high(PIN_TIMEOUT_US)) {
        return;
    }
}

}