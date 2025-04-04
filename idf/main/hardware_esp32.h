#ifndef __hardware_esp32_h__
#define __hardware_esp32_h__

//#include <Arduino.h>
// #include <pgmspace.h>

#include <stdint.h>
#include <hal/gpio_types.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/gpio_hal.h>
#include <hal/gpio_ll.h>
#include <esp_log.h>

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
extern gpio_dev_t *dev;

extern volatile uint32_t* gpio_low_set_reg;
extern volatile uint32_t* gpio_low_clear_reg;
extern volatile uint32_t* gpio_low_enable_set_reg;
extern volatile uint32_t* gpio_low_enable_clear_reg;

#define delay_ticks(ticks) vTaskDelay(ticks)

#ifdef CONFIG_IDF_TARGET_ESP32
#define LED_PIN 2
#define CS_PIN      4
#define MISO_PIN   19
#define MOSI_PIN   23
#define SCK_PIN    18

#define DATA0   32
#define DATA1   33
#define DATA2   25
#define DATA3   26
#define DATA4   27
#define DATA5   14
#define DATA6   12
#define DATA7   13

#define DATADIR 15

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

#define setInput(pin) gpio_hal_output_disable(&_gpio_hal, (gpio_num_t)pin)
#define setOutput(pin) gpio_hal_output_enable(&_gpio_hal, (gpio_num_t)pin)
#define digitalWrite2(pin,val) gpio_hal_set_level(&_gpio_hal, (gpio_num_t)pin, val)
#define digitalRead2(pin) gpio_hal_get_level(&_gpio_hal, (gpio_num_t)pin)

// note - only for GPIO < 32
#define setInputLL(pin) dev->enable_w1tc = (1 << pin)
#define setOutputLL(pin) dev->enable_w1ts = (1 << pin)
#define digitalWriteHighLL(pin) dev->out_w1ts = (1 << pin)
#define digitalWriteLowLL(pin) dev->out_w1tc = (1 << pin)
#define digitalReadLL(pin) ((dev->in >> pin) & 0x1)

#define setInputMask(mask) *gpio_low_enable_clear_reg = mask
#define setOutputMask(mask) *gpio_low_enable_set_reg = mask
#define digitalWriteHighMask(mask) *gpio_low_set_reg = mask
#define digitalWriteLowMask(mask) *gpio_low_clear_reg = mask

#define digitalReadMask(mask) (dev->in & mask)

#define lower_eoi()     digitalWriteLowMask(EOI_MASK)
#define lower_dav()     digitalWriteLowMask(DAV_MASK)
#define lower_nrfd()    digitalWriteLowMask(NRFD_MASK)
#define lower_ndac()    digitalWriteLowMask(NDAC_MASK)

#define raise_eoi()     digitalWriteHighMask(EOI_MASK)
#define raise_dav()     digitalWriteHighMask(DAV_MASK)
#define raise_nrfd()    digitalWriteHighMask(NRFD_MASK)
#define raise_ndac()    digitalWriteHighMask(NDAC_MASK)

#define set_eoi_output()     setOutputMask(EOI_MASK)
#define set_dav_output()     setOutputMask(DAV_MASK)
#define set_nrfd_output()    setOutputMask(NRFD_MASK)
#define set_ndac_output()    setOutputMask(NDAC_MASK)

#define read_atn()      digitalReadLL(ATN_PIN)
#define read_eoi()      digitalReadLL(EOI_PIN)
#define read_dav()      digitalReadLL(DAV_PIN)
#define read_nrfd()     digitalReadLL(NRFD_PIN)
#define read_ndac()     digitalReadLL(NDAC_PIN)

#define set_atn_input()     setInputLL(ATN_PIN)
#define set_eoi_input()     setInputLL(EOI_PIN)
#define set_dav_input()     setInputLL(DAV_PIN)
#define set_nrfd_input()    setInputLL(NRFD_PIN)
#define set_ndac_input()    setInputLL(NDAC_PIN)

#define set_datadir_output() setOutputLL(DATADIR)

#else
// esp32s2
#define LED_PIN 15
#define CS_PIN      4
#define MISO_PIN   37
#define MOSI_PIN   35
#define SCK_PIN    36

#define DATA0   5
#define DATA1   6
#define DATA2   7
#define DATA3   8
#define DATA4   9
#define DATA5   10
#define DATA6   11
#define DATA7   12

#define DATADIR 3

#define ATN_PIN     ((gpio_num_t)33)
#define EOI_PIN     ((gpio_num_t)2)
#define DAV_PIN     ((gpio_num_t)1)
#define NRFD_PIN    ((gpio_num_t)38)
//#define NDAC_PIN    ((gpio_num_t)16)
#define NDAC_PIN    ((gpio_num_t)34)

#define setInput(pin) gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT)
#define setOutput(pin) gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT)
#define digitalWrite2(pin,val) gpio_set_level((gpio_num_t)pin, val)
#define digitalRead2(pin) gpio_get_level((gpio_num_t)pin)

#define lower_eoi()     gpio_set_level(EOI_PIN, 0)
#define lower_dav()     gpio_set_level(DAV_PIN, 0)
#define lower_nrfd()    gpio_set_level(NRFD_PIN, 0)
#define lower_ndac()    gpio_set_level(NDAC_PIN, 0)

#define raise_eoi()     gpio_set_level(EOI_PIN, 1);
#define raise_dav()     gpio_set_level(DAV_PIN, 1)
#define raise_nrfd()    gpio_set_level(NRFD_PIN, 1)
#define raise_ndac()    gpio_set_level(NDAC_PIN, 1)

#define set_eoi_output()     setOutput(EOI_PIN)
#define set_dav_output()     setOutput(DAV_PIN)
#define set_nrfd_output()    setOutput(NRFD_PIN)
#define set_ndac_output()    setOutput(NDAC_PIN)

#define read_atn()      digitalRead2(ATN_PIN)
#define read_eoi()      digitalRead2(EOI_PIN)
#define read_dav()      digitalRead2(DAV_PIN)
#define read_nrfd()     digitalRead2(NRFD_PIN)
#define read_ndac()     digitalRead2(NDAC_PIN)

#define set_atn_input()     setInput(ATN_PIN)
#define set_eoi_input()     setInput(EOI_PIN)
#define set_dav_input()     setInput(DAV_PIN)
#define set_nrfd_input()    setInput(NRFD_PIN)
#define set_ndac_input()    setInput(NDAC_PIN)

#define set_datadir_output() setOutput(DATADIR)
#endif

// NOTE: DATA1 is the only used GPIO pin > 32
// this means the low level gpio functions can't be used since it
// requires interaction with the second gpio register

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

#define ieee_set_data_output() ({\
    digitalWrite2(DATADIR, HIGH);\
    setOutput(DATA0);\
    setOutput(DATA1);\
    setOutput(DATA2);\
    setOutput(DATA3);\
    setOutput(DATA4);\
    setOutput(DATA5);\
    setOutput(DATA6);\
    setOutput(DATA7);\
})

#define ieee_set_data_input() ({\
    setInput(DATA0);\
    setInput(DATA1);\
    setInput(DATA2);\
    setInput(DATA3);\
    setInput(DATA4);\
    setInput(DATA5);\
    setInput(DATA6);\
    setInput(DATA7);\
    digitalWrite2(DATADIR, LOW);\
})

#define enable_interrupts() portENABLE_INTERRUPTS()
#define disable_interrupts() portDISABLE_INTERRUPTS()

#define log_i(format, ...) ESP_LOGI("pd", format, ##__VA_ARGS__)
#define log_d(format, ...) ESP_LOGD("pd", format, ##__VA_ARGS__)
#define log_e(format, ...) ESP_LOGE("pd", format, ##__VA_ARGS__)

#define log_i_d(format, ...) enable_interrupts(); ESP_LOGI("pd", format, ##__VA_ARGS__); disable_interrupts()
#define log_d_d(format, ...) enable_interrupts(); ESP_LOGD("pd", format, ##__VA_ARGS__); disable_interrupts()
#define log_e_d(format, ...) enable_interrupts(); ESP_LOGE("pd", format, ##__VA_ARGS__); disable_interrupts()

#endif