#ifndef __IEEE488_HARDWARE_H__
#define __IEEE488_HARDWARE_H__

#include <Arduino.h>

#define DATA0   32
#define DATA1   33
#define DATA2   25
#define DATA3   26
#define DATA4   27
#define DATA5   14
#define DATA6   12
#define DATA7   13

#define DATADIR 15

#define ATN_PIN     36
#define EOI_PIN     22
#define DAV_PIN     21
#define NRFD_PIN    17
#define NDAC_PIN    16

#define lower_eoi()     digitalWrite(EOI_PIN, LOW)
#define lower_dav()     digitalWrite(DAV_PIN, LOW)
#define lower_nrfd()    digitalWrite(NRFD_PIN, LOW)
#define lower_ndac()    digitalWrite(NDAC_PIN, LOW)

#define raise_eoi()     digitalWrite(EOI_PIN, HIGH)
#define raise_dav()     digitalWrite(DAV_PIN, HIGH)
#define raise_nrfd()    digitalWrite(NRFD_PIN, HIGH)
#define raise_ndac()    digitalWrite(NDAC_PIN, HIGH)

#define read_atn()      digitalRead(ATN_PIN)
#define read_eoi()      digitalRead(EOI_PIN)
#define read_dav()      digitalRead(DAV_PIN)
#define read_nrfd()     digitalRead(NRFD_PIN)
#define read_ndac()     digitalRead(NDAC_PIN)

#define set_atn_input()     pinMode(ATN_PIN, INPUT_PULLUP)
#define set_eoi_input()     pinMode(EOI_PIN, INPUT_PULLUP)
#define set_dav_input()     pinMode(DAV_PIN, INPUT_PULLUP)
#define set_nrfd_input()    pinMode(NRFD_PIN, INPUT_PULLUP)
#define set_ndac_input()    pinMode(NDAC_PIN, INPUT_PULLUP)

#define set_eoi_output()     pinMode(EOI_PIN, OUTPUT)
#define set_dav_output()     pinMode(DAV_PIN, OUTPUT)
#define set_nrfd_output()    pinMode(NRFD_PIN, OUTPUT)
#define set_ndac_output()    pinMode(NDAC_PIN, OUTPUT)
#define set_datadir_output() pinMode(DATADIR, OUTPUT)

#define ieee_read_data_byte(recvByte) ({\
    recvByte += digitalRead(DATA7); recvByte <<= 1;\
    recvByte += digitalRead(DATA6); recvByte <<= 1;\
    recvByte += digitalRead(DATA5); recvByte <<= 1;\
    recvByte += digitalRead(DATA4); recvByte <<= 1;\
    recvByte += digitalRead(DATA3); recvByte <<= 1;\
    recvByte += digitalRead(DATA2); recvByte <<= 1;\
    recvByte += digitalRead(DATA1); recvByte <<= 1;\
    recvByte += digitalRead(DATA0);\
})

#define ieee_write_data_byte(byte) ({\
    digitalWrite(DATA0, byte & 0x01); byte >>= 1;\
    digitalWrite(DATA1, byte & 0x01); byte >>= 1;\
    digitalWrite(DATA2, byte & 0x01); byte >>= 1;\
    digitalWrite(DATA3, byte & 0x01); byte >>= 1;\
    digitalWrite(DATA4, byte & 0x01); byte >>= 1;\
    digitalWrite(DATA5, byte & 0x01); byte >>= 1;\
    digitalWrite(DATA6, byte & 0x01); byte >>= 1;\
    digitalWrite(DATA7, byte & 0x01);\
})

#define ieee_set_data_output() ({\
    digitalWrite(DATADIR, HIGH);\
    pinMode(DATA0, OUTPUT);\
    pinMode(DATA1, OUTPUT);\
    pinMode(DATA2, OUTPUT);\
    pinMode(DATA3, OUTPUT);\
    pinMode(DATA4, OUTPUT);\
    pinMode(DATA5, OUTPUT);\
    pinMode(DATA6, OUTPUT);\
    pinMode(DATA7, OUTPUT);\
})

#define ieee_set_data_input() ({\
    pinMode(DATA0, INPUT_PULLUP);\
    pinMode(DATA1, INPUT_PULLUP);\
    pinMode(DATA2, INPUT_PULLUP);\
    pinMode(DATA3, INPUT_PULLUP);\
    pinMode(DATA4, INPUT_PULLUP);\
    pinMode(DATA5, INPUT_PULLUP);\
    pinMode(DATA6, INPUT_PULLUP);\
    pinMode(DATA7, INPUT_PULLUP);\
    digitalWrite(DATADIR, LOW);\
})

#endif