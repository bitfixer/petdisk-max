#ifndef _SPI_ROUTINES_H_
#define _SPI_ROUTINES_H_

#include <stdint.h>

class bSPI {
public:
    bSPI() {}
    ~bSPI() {}

    void init();
    uint8_t transmit(uint8_t data);
    uint8_t receive();

    void cs_select();
    void cs_unselect();
};

#endif
