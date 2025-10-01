#ifndef __hardware_esp32_h__
#define __hardware_esp32_h__

#include <stdint.h>
#include <hal/gpio_types.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/gpio_hal.h>
#include <hal/gpio_ll.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "esp-fast-gpio.h"

#define LOW     0x0
#define HIGH    0x1
#define OUTPUT  GPIO_MODE_OUTPUT
#define INPUT_PULLUP GPIO_MODE_INPUT

#define sscanf_P sscanf
#define sprintf_P sprintf

#define PSTR(str) str
#define PROGMEM

void hardware_cmd_init();
void gpio_init();

#define delay_ticks(ticks) vTaskDelay(ticks)

#ifdef CONFIG_IDF_TARGET_ESP32
#define LED_PIN     2
#define CS_PIN      4
#define MISO_PIN    19
#define MOSI_PIN    23
#define SCK_PIN     18

#define DATA0       32
#define DATA0_HI    0
#define DATA1       33
#define DATA1_HI    1
#define DATA2       25
#define DATA3       26
#define DATA4       27
#define DATA5       14
#define DATA6       12
#define DATA7       13

#define DATA_LOW_MASK   0xE007000
#define DATA_HIGH_MASK  0x3

#define DATADIR     15

#define ATN_PIN     5
#define EOI_PIN     22
#define DAV_PIN     21
#define NRFD_PIN    17
#define NDAC_PIN    16
#define NDAC_MASK   0b10000000000000000

extern uint32_t data_mask_low[256];
extern uint32_t data_mask_hi[256];

#else
// esp32s2
#define LED_PIN     15
#define CS_PIN      4
#define MISO_PIN    37
#define MOSI_PIN    35
#define SCK_PIN     36

#define DATA0       5
#define DATA1       6
#define DATA2       7
#define DATA3       8
#define DATA4       9
#define DATA5       10
#define DATA6       11
#define DATA7       12

#define DATADIR     3

#define ATN_PIN     18
#define EOI_PIN     2
#define DAV_PIN     1
#define NRFD_PIN    21
#define NDAC_PIN    16
#define NDAC_MASK   0b10000000000000000

#define DATA_MASK   0b1111111100000

#endif

#define lower_eoi()         fast_gpio_set_low(EOI_PIN)
#define lower_dav()         fast_gpio_set_low(DAV_PIN)
#define lower_nrfd()        fast_gpio_set_low(NRFD_PIN)
#define lower_ndac()        fast_gpio_set_low_mask(NDAC_MASK)

#define raise_eoi()         fast_gpio_set_high(EOI_PIN)
#define raise_dav()         fast_gpio_set_high(DAV_PIN)
#define raise_nrfd()        fast_gpio_set_high(NRFD_PIN)
#define raise_ndac()        fast_gpio_set_high_mask(NDAC_MASK)

#define set_eoi_output()    fast_gpio_set_output(EOI_PIN)
#define set_dav_output()    fast_gpio_set_output(DAV_PIN)
#define set_nrfd_output()   fast_gpio_set_output(NRFD_PIN)
#define set_ndac_output()   fast_gpio_set_output(NDAC_PIN)

#define read_atn()          fast_gpio_get(ATN_PIN)
#define read_eoi()          fast_gpio_get(EOI_PIN)
#define read_dav()          fast_gpio_get(DAV_PIN)
#define read_nrfd()         fast_gpio_get(NRFD_PIN)
#define read_ndac()         fast_gpio_get(NDAC_PIN)

#define set_atn_input()     fast_gpio_set_input(ATN_PIN)
#define set_eoi_input()     fast_gpio_set_input(EOI_PIN)
#define set_dav_input()     fast_gpio_set_input(DAV_PIN)
#define set_nrfd_input()    fast_gpio_set_input(NRFD_PIN)
#define set_ndac_input()    fast_gpio_set_input(NDAC_PIN)
//#define set_ndac_input()    raise_ndac()

#define set_datadir_output()    fast_gpio_set_output(DATADIR)

#define raise_datadir()     fast_gpio_set_high(DATADIR)
#define lower_datadir()     fast_gpio_set_low(DATADIR)

#ifdef CONFIG_IDF_TARGET_ESP32

#define ieee_read_data_byte(recvByte) ({\
    recvByte += fast_gpio_get(DATA7); recvByte <<= 1;\
    recvByte += fast_gpio_get(DATA6); recvByte <<= 1;\
    recvByte += fast_gpio_get(DATA5); recvByte <<= 1;\
    recvByte += fast_gpio_get(DATA4); recvByte <<= 1;\
    recvByte += fast_gpio_get(DATA3); recvByte <<= 1;\
    recvByte += fast_gpio_get(DATA2); recvByte <<= 1;\
    recvByte += fast_gpio_get_high(DATA1_HI); recvByte <<= 1;\
    recvByte += fast_gpio_get_high(DATA0_HI);\
})

#define ieee_write_data_byte(byte) ({\
    fast_gpio_write_mask(data_mask_low[byte], DATA_LOW_MASK);\
    fast_gpio_write_mask_high(data_mask_hi[byte], DATA_HIGH_MASK);\
})

#define ieee_set_data_output() ({\
    raise_datadir();\
    fast_gpio_set_output_mask(DATA_LOW_MASK);\
    fast_gpio_set_output_mask_high(DATA_HIGH_MASK);\
})

#define ieee_set_data_input() ({\
    fast_gpio_set_input_mask(DATA_LOW_MASK);\
    fast_gpio_set_input_mask_high(DATA_HIGH_MASK);\
    lower_datadir();\
})

#define read_miso() fast_gpio_get(MISO_PIN)

#else
// esp32s2
#define ieee_read_data_byte(recvByte)   recvByte = fast_gpio_read_byte(DATA0)
#define ieee_write_data_byte(byte)      fast_gpio_write_byte(byte, DATA0)

#define ieee_set_data_output() ({\
    raise_datadir();\
    fast_gpio_set_output_mask(DATA_MASK);\
})

#define ieee_set_data_input() ({\
    fast_gpio_set_input_mask(DATA_MASK);\
    lower_datadir();\
})

#define read_miso()     fast_gpio_get_high(MISO_PIN-32)
#endif

#define enable_interrupts() portENABLE_INTERRUPTS()
#define disable_interrupts() portDISABLE_INTERRUPTS()

#define log_i(format, ...) ESP_LOGI("pd", format, ##__VA_ARGS__)
#define log_d(format, ...) ESP_LOGD("pd", format, ##__VA_ARGS__)
#define log_e(format, ...) ESP_LOGE("pd", format, ##__VA_ARGS__)

#define log_i_d(format, ...) enable_interrupts(); ESP_LOGI("pd", format, ##__VA_ARGS__); disable_interrupts()
#define log_d_d(format, ...) enable_interrupts(); ESP_LOGD("pd", format, ##__VA_ARGS__); disable_interrupts()
#define log_e_d(format, ...) enable_interrupts(); ESP_LOGE("pd", format, ##__VA_ARGS__); disable_interrupts()

#define get_time_us()   esp_timer_get_time()

void setup_atn_interrupt();
void wait_atn_isr();
void clear_atn();

#endif