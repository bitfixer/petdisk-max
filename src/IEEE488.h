/*
    IEEE488.h
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

#ifndef _IEEE488_H
#define _IEEE488_H

#include "SerialLogger.h"

#define IEEE_PORT   PORTC
#define IEEE_PIN    PINC
#define IEEE_CTL    DDRC

#define DATA_PORT   PORTA
#define DATA_PIN    PINA
#define DATA_CTL    DDRA

#define ATN     PC0
#define NDAC    PC1
#define DAV     PC2
#define NRFD    PC3
#define EOI     PC4

#define ATN_MASK        1<<ATN
#define DAV_MASK        1<<DAV
#define EOI_MASK        1<<EOI

#define NRFD_MASK       1<<NRFD
#define NOT_NRFD_MASK   ~(NRFD_MASK)

#define NDAC_MASK       1<<NDAC
#define NOT_NDAC_MASK   ~(NDAC_MASK)

#define DATA0   0x01
#define DATA1   0x02

#define TALK        0x40
#define LISTEN      0x20

#define UNTALK      0x5F
#define UNLISTEN    0x3F

class IEEE488 {
public:
    IEEE488(SerialLogger* logger)
    : _logger(logger)
    , _busval(0)
    {}

    ~IEEE488() {}

    void sendIEEEBytes(unsigned char *entry, int size, unsigned char isLast);
    unsigned char sendIEEEByteCheckForATN(unsigned char byte);
    unsigned char sendIEEEByteCheckForATN2(unsigned char byte, bool last);
    unsigned char wait_for_device_address(unsigned char my_address);
    void unlisten();
    void begin_output();
    void end_output();

    unsigned char get_byte_from_bus();
    void acknowledge_bus_byte();
    bool atn_is_low();
    bool eoi_is_low();
    void signal_ready_for_data();

    void raise_dav_and_eoi();
    unsigned char wait_for_ndac_low_or_atn_low();
    unsigned char wait_for_ndac_high_or_atn_low();
    unsigned char wait_for_nrfd_high_or_atn_low();

    unsigned char get_device_address(unsigned char* dir);
    void accept_address();
    void reject_address();
private:
    SerialLogger* _logger;
    unsigned char _busval;

    void lower_nrfd();
    void raise_nrfd();
    void lower_dav();
    void raise_dav();

    void wait_for_atn_high();
    void wait_for_dav_low();
    void wait_for_dav_high();

    void wait_for_atn_low();
    void wait_for_nrfd_high();
    void wait_for_ndac_high();
    void wait_for_ndac_low();
    void send_byte(unsigned char byte, int last);
    void set_nrfd_input();
    void set_ndac_input();
    void set_dav_output();
    void set_eoi_output();
    void set_data_output();
    void set_data_input();

    void recv_byte(unsigned char *byte);

    void lower_ndac();
    void raise_ndac();

};

#endif