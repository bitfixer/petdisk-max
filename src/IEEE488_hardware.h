#ifndef __IEEE488_HARDWARE_H__
#define __IEEE488_HARDWARE_H__

#include <avr/io.h>

#define IEEE_PORT   PORTC
#define IEEE_PIN    PINC
#define IEEE_CTL    DDRC

#define DATA_PORT   PORTA
#define DATA_PIN    PINA
#define DATA_CTL    DDRA

#define ATN_PIN     PC0
#define NDAC_PIN    PC1
#define DAV_PIN     PC2
#define NRFD_PIN    PC3
#define EOI_PIN     PC4

#define ATN_MASK        1<<ATN_PIN
#define NOT_ATN_MASK    ~(ATN_MASK)

#define DAV_MASK        1<<DAV_PIN
#define NOT_DAV_MASK    ~(DAV_MASK)

#define EOI_MASK        1<<EOI_PIN
#define NOT_EOI_MASK    ~(EOI_MASK)

#define NRFD_MASK       1<<NRFD_PIN
#define NOT_NRFD_MASK   ~(NRFD_MASK)

#define NDAC_MASK       1<<NDAC_PIN
#define NOT_NDAC_MASK   ~(NDAC_MASK)

#define lower_eoi()     IEEE_PORT &= NOT_EOI_MASK
#define lower_dav()     IEEE_PORT &= NOT_DAV_MASK
#define lower_nrfd()    IEEE_PORT &= NOT_NRFD_MASK
#define lower_ndac()    IEEE_PORT &= NOT_NDAC_MASK

#define raise_eoi()     IEEE_PORT |= EOI_MASK
#define raise_dav()     IEEE_PORT |= DAV_MASK
#define raise_nrfd()    IEEE_PORT |= NRFD_MASK
#define raise_ndac()    IEEE_PORT |= NDAC_MASK

#define read_atn()      (IEEE_PIN & ATN_MASK)
#define read_eoi()      (IEEE_PIN & EOI_MASK)
#define read_dav()      (IEEE_PIN & DAV_MASK)
#define read_nrfd()     (IEEE_PIN & NRFD_MASK)
#define read_ndac()     (IEEE_PIN & NDAC_MASK)

#define set_atn_input()     IEEE_CTL &= NOT_ATN_MASK
#define set_eoi_input()     IEEE_CTL &= NOT_EOI_MASK
#define set_dav_input()     IEEE_CTL &= NOT_DAV_MASK
#define set_nrfd_input()    IEEE_CTL &= NOT_NRFD_MASK
#define set_ndac_input()    IEEE_CTL &= NOT_NDAC_MASK

#define set_eoi_output()     IEEE_CTL |= EOI_MASK
#define set_dav_output()     IEEE_CTL |= DAV_MASK
#define set_nrfd_output()    IEEE_CTL |= NRFD_MASK
#define set_ndac_output()    IEEE_CTL |= NDAC_MASK
#define set_datadir_output()

#define ieee_read_data_byte(recvByte)   recvByte = DATA_PIN
#define ieee_write_data_byte(byte)      DATA_PORT = byte
#define ieee_set_data_output()          DATA_CTL = 0xFF
#define ieee_set_data_input()           DATA_CTL = 0x00


#endif