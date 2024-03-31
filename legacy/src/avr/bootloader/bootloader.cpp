#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/boot.h>
#include <stdio.h>
#include <string.h>

#include "SPI_routines.h"
#include "SD_routines.h"
#include "FAT32_tiny.h"

#include <avr/pgmspace.h>

// offset should be 2*boot start word address
// using 2048 word bootloader, boot start address is $3800 (word)
// this is 0x7000 in bytes
// linker param: -Ttext=0x7000

// UART init
void serial_init(unsigned int ubrr)
{
    UBRR1H = (unsigned char)(ubrr>>8);
    UBRR1L = (unsigned char)ubrr;
    UCSR1A = 0x00;
    UCSR1B = (1<<RXEN1)|(1<<TXEN1);
    UCSR1C = (1<<USBS1)|(3<<UCSZ10);
}

void serial_transmitByte(unsigned char data)
{
    while ( !(UCSR1A & (1<<UDRE1)) ) {}
    UDR1 = data;
}

void boot_program_page (uint32_t page, uint8_t *buf)
{
    uint16_t i;
    uint8_t sreg;

    // Disable interrupts.

    sreg = SREG;
    cli();

    eeprom_busy_wait ();

    boot_page_erase (page);
    boot_spm_busy_wait ();      // Wait until the memory is erased.

    for (i = 0; i < SPM_PAGESIZE; i += 2)
    {
        // Set up little-endian word.

        uint16_t w = *buf++;
        w += (*buf++) << 8;

        boot_page_fill (page + i, w);
    }

    boot_page_write (page);     // Store buffer in flash page.
    boot_spm_busy_wait();       // Wait until the memory is written.

    // Reenable RWW-section again. We need this if we want to jump back
    // to the application after bootloading.

    boot_rww_enable ();

    // Re-enable interrupts (if they were ever enabled).

    SREG = sreg;
}

void enable_led()
{
    DDRB |= 1 << PB0;
    DDRB |= 1 << PB1;
}

void set_led(bool value)
{
    if (value == true)
    {
        PORTB |= 1 << PB0;
        PORTB |= 1 << PB1;
    }
    else
    {
        PORTB &= ~(1 << PB0);
        PORTB &= ~(1 << PB1);
    }
}

void blink_led(int count, int ms_on, int ms_off)
{
    set_led(false);
    for (int i = 0; i < count; i++)
    {
        set_led(true);
        for (int j = 0; j < ms_on; j++)
        {
            _delay_ms(1);
        }
        set_led(false);
        for (int j = 0; j < ms_off; j++)
        {
            _delay_ms(1);
        }
    }
}


void load_firmware(void)
{
    unsigned char buffer[512];
    unsigned int bytes;
    uint32_t page;

    SPI spi;
    spi.init();

    SD sd(&spi, SPI_CS);

    serial_init(0);
    serial_transmitByte('*');
    serial_transmitByte('\r');
    serial_transmitByte('\n');

    enable_led();
    set_led(false);

    unsigned char ret = sd.init();

    if (ret == 0)
    {
        
        // get files in root dir
        FAT32 fat32(&sd, buffer);
        fat32.init();

        fat32.openDirectory();

        while (fat32.getNextDirectoryEntry() == true)
        {
            unsigned char* name = fat32.getShortFilename();
            for (int i = 0; i < 11; i++)
            {
                serial_transmitByte(name[i]);
            }

            // check for a filename of the type FIRM****.BIN
            // the first file with this pattern will be written to memory
            if (name[0] == 'F' && 
                name[1] == 'I' && 
                name[2] == 'R' && 
                name[3] == 'M' &&
                name[8] == 'B' &&
                name[9] == 'I' &&
                name[10] == 'N')
            {
                blink_led(2, 150, 150);
                // found firmware file
                serial_transmitByte('>');
                for (int i = 0; i < 11; i++)
                {
                    serial_transmitByte(name[i]);
                }

                fat32.openFileForReading();
                bytes = fat32.getNextFileBlock();
                page = 0;
                while (bytes > 0)
                {
                    for (int i = 0; i < 512; i += SPM_PAGESIZE)
                    {
                        boot_program_page(page, &buffer[i]);
                        page += SPM_PAGESIZE;
                    }

                    bytes = fat32.getNextFileBlock();
                }

                break;
            }

            serial_transmitByte('\r');
            serial_transmitByte('\n');
        }
        blink_led(1, 150, 150);
    }
    else
    {
        blink_led(3, 150, 150);
    }
}

int main()
{
    load_firmware();
    asm("jmp 0");
}