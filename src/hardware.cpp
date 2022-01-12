#include "hardware.h"

#include <stdio.h>
#include <ctype.h>
#include <util/delay.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <avr/pgmspace.h>

#define ESP_CONTROL     DDRD
#define ESP_PORT        PORTD
#define ESP_CH_PD       PD4
#define ESP_RST         PD5
#define ESP_GPIO0       PD7
#define ESP_GPIO2       PD6

void prog_init()
{
     // set reset for esp
    ESP_PORT = (1 << ESP_CH_PD) | (1 << ESP_RST);
    ESP_CONTROL = (1 << ESP_CH_PD) | (1 << ESP_RST);
    sei();
}

void reset_esp()
{
    ESP_PORT &= ~(1 << ESP_RST);
    ESP_PORT &= ~(1 << ESP_CH_PD);

    for (int i = 0; i < 20; i++)
    {
        _delay_loop_2(65535);
    }

    ESP_PORT |= 1 << ESP_RST;
    ESP_PORT |= 1 << ESP_CH_PD;
}

void init_led()
{

}

void set_led(bool value)
{

}

void hDelayMs(int ms)
{
    //_delay_ms(ms);
}