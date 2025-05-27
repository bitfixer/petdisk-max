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

extern gpio_hal_context_t _gpio_hal;

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

#define DATADIR     15
#define DATADIR_MASK 0b1000000000000000

#define ATN_PIN     5
#define ATN_MASK    0b00000000000000000000000000100000
#define EOI_PIN     22
#define EOI_MASK    0b00000000010000000000000000000000
#define DAV_PIN     21
#define DAV_MASK    0b00000000001000000000000000000000
#define NRFD_PIN    17
#define NRFD_MASK   0b00000000000000100000000000000000
#define NDAC_PIN    16
#define NDAC_MASK   0b00000000000000010000000000000000

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

#define DATA_MASK   0b1111111100000

// NOTE: old definitions
//#define ATN_PIN     ((gpio_num_t)33)
//#define EOI_PIN     ((gpio_num_t)2)
//#define DAV_PIN     ((gpio_num_t)1)
//#define NRFD_PIN    ((gpio_num_t)38)
//#define NDAC_PIN    ((gpio_num_t)34)

#endif

#define setPinInput(pin) gpio_hal_output_disable(&_gpio_hal, (gpio_num_t)pin)
#define setPinOutput(pin) gpio_hal_output_enable(&_gpio_hal, (gpio_num_t)pin)
#define digitalWrite2(pin,val) gpio_hal_set_level(&_gpio_hal, (gpio_num_t)pin, val)
#define digitalRead2(pin) gpio_hal_get_level(&_gpio_hal, (gpio_num_t)pin)

#define lower_eoi()         EspFastGpio::setLow(EOI_PIN)
#define lower_dav()         EspFastGpio::setLow(DAV_PIN)
#define lower_nrfd()        EspFastGpio::setLow(NRFD_PIN)
#define lower_ndac()        EspFastGpio::setLow(NDAC_PIN)

#define raise_eoi()         EspFastGpio::setHigh(EOI_PIN)
#define raise_dav()         EspFastGpio::setHigh(DAV_PIN)
#define raise_nrfd()        EspFastGpio::setHigh(NRFD_PIN)
#define raise_ndac()        EspFastGpio::setHigh(NDAC_PIN)

#define set_eoi_output()    EspFastGpio::setOutput(EOI_PIN)
#define set_dav_output()    EspFastGpio::setOutput(DAV_PIN)
#define set_nrfd_output()   EspFastGpio::setOutput(NRFD_PIN)
#define set_ndac_output()   EspFastGpio::setOutput(NDAC_PIN)

#define read_atn()          EspFastGpio::get(ATN_PIN)
#define read_eoi()          EspFastGpio::get(EOI_PIN)
#define read_dav()          EspFastGpio::get(DAV_PIN)
#define read_nrfd()         EspFastGpio::get(NRFD_PIN)
#define read_ndac()         EspFastGpio::get(NDAC_PIN)

#define set_atn_input()     EspFastGpio::setInput(ATN_PIN)
#define set_eoi_input()     EspFastGpio::setInput(EOI_PIN)
#define set_dav_input()     EspFastGpio::setInput(DAV_PIN)
#define set_nrfd_input()    EspFastGpio::setInput(NRFD_PIN)
#define set_ndac_input()    EspFastGpio::setInput(NDAC_PIN)

#define set_datadir_output()    EspFastGpio::setOutput(DATADIR)

#define raise_datadir()     EspFastGpio::setHigh(DATADIR)
#define lower_datadir()     EspFastGpio::setLow(DATADIR)

#ifdef CONFIG_IDF_TARGET_ESP32
/*
#define ieee_read_data_byte(recvByte) ({\
    recvByte += digitalRead2(DATA7); recvByte <<= 1;\
    recvByte += digitalRead2(DATA6); recvByte <<= 1;\
    recvByte += digitalRead2(DATA5); recvByte <<= 1;\
    recvByte += digitalRead2(DATA4); recvByte <<= 1;\
    recvByte += digitalRead2(DATA3); recvByte <<= 1;\
    recvByte += digitalRead2(DATA2); recvByte <<= 1;\
    recvByte += digitalRead2(DATA1); recvByte <<= 1;\
    recvByte += digitalRead2(DATA0);\
})
*/

#define ieee_read_data_byte(recvByte) ({\
    recvByte += EspFastGpio::get(DATA7); recvByte <<= 1;\
    recvByte += EspFastGpio::get(DATA6); recvByte <<= 1;\
    recvByte += EspFastGpio::get(DATA5); recvByte <<= 1;\
    recvByte += EspFastGpio::get(DATA4); recvByte <<= 1;\
    recvByte += EspFastGpio::get(DATA3); recvByte <<= 1;\
    recvByte += EspFastGpio::get(DATA2); recvByte <<= 1;\
    recvByte += EspFastGpio::get_high(DATA1_HI); recvByte <<= 1;\
    recvByte += EspFastGpio::get_high(DATA0_HI);\
})

#define ieee_write_data_byte(byte) ({\
    digitalWrite2(DATA0, byte & 0x01); byte >>= 1;\
    digitalWrite2(DATA1, byte & 0x01); byte >>= 1;\
    digitalWrite2(DATA2, byte & 0x01); byte >>= 1;\
    digitalWrite2(DATA3, byte & 0x01); byte >>= 1;\
    digitalWrite2(DATA4, byte & 0x01); byte >>= 1;\
    digitalWrite2(DATA5, byte & 0x01); byte >>= 1;\
    digitalWrite2(DATA6, byte & 0x01); byte >>= 1;\
    digitalWrite2(DATA7, byte & 0x01);\
})

/*
#define ieee_write_data_byte(byte) ({\
    EspFastGpio::writeMask(data_mask_low[byte]);\
    EspFastGpio::writeMaskHigh(data_mask_hi[byte]);\
})
*/

#define ieee_set_data_output() ({\
    raise_datadir();\
    EspFastGpio::setOutputHigh(DATA0_HI);\
    EspFastGpio::setOutputHigh(DATA1_HI);\
    EspFastGpio::setOutput(DATA2);\
    EspFastGpio::setOutput(DATA3);\
    EspFastGpio::setOutput(DATA4);\
    EspFastGpio::setOutput(DATA5);\
    EspFastGpio::setOutput(DATA6);\
    EspFastGpio::setOutput(DATA7);\
})

#define ieee_set_data_input() ({\
    EspFastGpio::setInputHigh(DATA0_HI);\
    EspFastGpio::setInputHigh(DATA1_HI);\
    EspFastGpio::setInput(DATA2);\
    EspFastGpio::setInput(DATA3);\
    EspFastGpio::setInput(DATA4);\
    EspFastGpio::setInput(DATA5);\
    EspFastGpio::setInput(DATA6);\
    EspFastGpio::setInput(DATA7);\
    lower_datadir();\
})
#else
// esp32s2
#define ieee_read_data_byte(recvByte)   recvByte = EspFastGpio::readByte(DATA0)
#define ieee_write_data_byte(byte)      EspFastGpio::writeByte(byte, DATA0)

#define ieee_set_data_output() ({\
    raise_datadir();\
    EspFastGpio::setOutputMask(DATA_MASK);\
})

#define ieee_set_data_input() ({\
    EspFastGpio::setInputMask(DATA_MASK);\
    lower_datadir();\
})
#endif

#define enable_interrupts() portENABLE_INTERRUPTS()
#define disable_interrupts() portDISABLE_INTERRUPTS()

#define log_i(format, ...) ESP_LOGI("pd", format, ##__VA_ARGS__)
#define log_d(format, ...) ESP_LOGD("pd", format, ##__VA_ARGS__)
#define log_e(format, ...) ESP_LOGE("pd", format, ##__VA_ARGS__)

#define log_i_d(format, ...) enable_interrupts(); ESP_LOGI("pd", format, ##__VA_ARGS__); disable_interrupts()
#define log_d_d(format, ...) enable_interrupts(); ESP_LOGD("pd", format, ##__VA_ARGS__); disable_interrupts()
#define log_e_d(format, ...) enable_interrupts(); ESP_LOGE("pd", format, ##__VA_ARGS__); disable_interrupts()

void setup_atn_interrupt();
void wait_atn_isr();
void clear_atn();

#endif