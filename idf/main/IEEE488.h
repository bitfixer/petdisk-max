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

#include <stdint.h>
#include <stddef.h>

#define TALK        0x40
#define LISTEN      0x20

#define UNTALK      0x5F
#define UNLISTEN    0x3F

typedef enum {
    ATN = 0x01,
    NDAC = 0x02,
    DAV = 0x04,
    NRFD = 0x08,
    EOI = 0x10
} IEEEBusSignal;

namespace bitfixer {

class IEEE488 {
public:
    IEEE488()
    : _atn_low(false)
    , _eoi_low(false)
    , _unlistened(false)
    {}

    ~IEEE488() {}

    void init();
    void sendIEEEBytes(uint8_t *entry, int size, uint8_t isLast);
    uint8_t sendIEEEByteCheckForATN(uint8_t byte);
    uint8_t sendIEEEByteCheckForATN2(uint8_t byte, bool last);
    void unlisten();
    bool is_unlistened();
    void begin_output();

    void begin_output_start();
    void begin_output_end();

    void end_output();

    uint8_t get_byte_from_bus();
    void acknowledge_bus_byte();
    bool atn_is_low();
    bool eoi_is_low();
    void signal_ready_for_data();

    void raise_dav_and_eoi();
    uint8_t wait_for_ndac_low_or_atn_low();
    IEEEBusSignal wait_for_ndac_high_or_atn_low();
    uint8_t wait_for_nrfd_high_or_atn_low();
    void wait_for_atn_low();
    bool wait_for_dav_low(int timeout_us);
    void recv_byte(uint8_t *byte);

    uint8_t get_device_address(uint8_t* dir, bool* success);
    void accept_address();
    void reject_address();

    void write_byte_to_data_bus(uint8_t byte);

    static IEEE488* get_instance();
private:
    bool _atn_low;
    bool _eoi_low;

    bool _unlistened;

    void wait_for_atn_high();
    void wait_for_atn_high_with_timeout(int timeout_us);
    void wait_for_dav_high();

    void wait_for_nrfd_high();
    void wait_for_nrfd_low();
    void wait_for_ndac_high();
    void wait_for_ndac_low();
    void send_byte(uint8_t byte, int last);

};

}

#endif