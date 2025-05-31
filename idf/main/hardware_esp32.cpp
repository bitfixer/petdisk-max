#include "hardware.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <driver/spi_master.h>
#include <freertos/semphr.h>

#include <hal/gpio_hal.h>

#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_timer.h>
#include <nvs.h>
#include <esp_cpu.h>

#include "console.h"
#include "IEEE488.h"

static const char* TAG = "hw";

#ifdef CONFIG_IDF_TARGET_ESP32
uint32_t data_mask_low[256];
uint32_t data_mask_hi[256];
#endif

void gpio_init() {
    // set all used io pins as output
    //zero-initialize the config structure.
    gpio_config_t io_conf;
    memset(&io_conf, 0, sizeof(gpio_config_t));
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as input mode
    io_conf.mode = (gpio_mode_t)INPUT_PULLUP;
    //bit mask of the pins that you want to set,e.g.GPIO18/19

    io_conf.pin_bit_mask = 1ULL << DATA0;
    io_conf.pin_bit_mask |= 1ULL << DATA1;
    io_conf.pin_bit_mask |= 1ULL << DATA2;
    io_conf.pin_bit_mask |= 1ULL << DATA3;
    io_conf.pin_bit_mask |= 1ULL << DATA4;
    io_conf.pin_bit_mask |= 1ULL << DATA5;
    io_conf.pin_bit_mask |= 1ULL << DATA6;
    io_conf.pin_bit_mask |= 1ULL << DATA7;

    io_conf.pin_bit_mask |= 1ULL << DATADIR;

    io_conf.pin_bit_mask |= 1ULL << ATN_PIN;
    io_conf.pin_bit_mask |= 1ULL << EOI_PIN;
    io_conf.pin_bit_mask |= 1ULL << DAV_PIN;
    io_conf.pin_bit_mask |= 1ULL << NRFD_PIN;
    io_conf.pin_bit_mask |= 1ULL << NDAC_PIN;

    io_conf.pin_bit_mask |= 1ULL << LED_PIN;

    //disable pull-down mode
    io_conf.pull_down_en = (gpio_pulldown_t)0;
    //disable pull-up mode
    io_conf.pull_up_en = (gpio_pullup_t)1;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    #ifdef CONFIG_IDF_TARGET_ESP32
    int data_pins[8];
    data_pins[0] = DATA0;
    data_pins[1] = DATA1;
    data_pins[2] = DATA2;
    data_pins[3] = DATA3;
    data_pins[4] = DATA4;
    data_pins[5] = DATA5;
    data_pins[6] = DATA6;
    data_pins[7] = DATA7;

    int pin_order[8];
    pin_order[0] = 0;
    pin_order[1] = 1;
    pin_order[2] = 6;
    pin_order[3] = 7;
    pin_order[4] = 5;
    pin_order[5] = 2;
    pin_order[6] = 3;
    pin_order[7] = 4;

    // prepare data masks, high and low registers
    for (int i = 0; i < 256; i++) {
        uint32_t low_mask = 0;
        uint32_t high_mask = 0;

        uint8_t byte = (uint8_t)i;
        for (int i = 0; i < 8; i++) {
            int data_pin = data_pins[i];
            if (byte & 0x1) {
                if (data_pin >= 32) {
                    high_mask |= (1 << (data_pin-32));
                } else {
                    low_mask |= (1 << data_pin);
                }
            }
            byte >>= 1;
        }

        ESP_LOGV(TAG, "datamask %X: %lX %lX", i, low_mask, high_mask);

        data_mask_low[i] = low_mask;
        data_mask_hi[i] = high_mask;
    }
    #endif

    EspFastGpio::init();
}

void setPinMode(int pin, int mode) {
    //zero-initialize the config structure.
    gpio_config_t io_conf;
    memset(&io_conf, 0, sizeof(gpio_config_t));
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = (gpio_mode_t)mode;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = 1ULL << pin;
    //disable pull-down mode
    io_conf.pull_down_en = (gpio_pulldown_t)0;
    //disable pull-up mode
    io_conf.pull_up_en = (gpio_pullup_t)1;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
}

uint8_t spi_cs()
{
    return CS_PIN;
}

static nvs_handle_t eeprom_nvs_handle;
static uint8_t eeprom_data[512];

void prog_init()
{
    // nothing for esp32
    set_atn_input();

    // initialize nvs
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs init failed");
    }

    err = nvs_open("storage", NVS_READWRITE, &eeprom_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed");
    }

    // read eeprom data into ram
    size_t size = 512;
    err = nvs_get_blob(eeprom_nvs_handle, "eeprom", eeprom_data, &size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "eeprom_write_block: failed to read");
        memset(eeprom_data, 0, 512);
    }
}

void init_led()
{
    fast_gpio_set_output(LED_PIN);
}

void set_led(bool value)
{
    if (value == true)
    {
        fast_gpio_set_high(LED_PIN);
    }
    else
    {
        fast_gpio_set_low(LED_PIN);
    }
}

// ====
// test function
static int ledinit(int argc, char** argv) {
    printf("running init_led\n");
    init_led();
    return 0;
}

static int ledset(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: ledset <val>\n");
        return 1;
    }

    int v = atoi(argv[1]);
    printf("ledset %d\n", v);
    set_led(v == 0 ? false : true);
    return 0;
}

static int pdpm(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: pdpm <pin_name> <mode>\n");
        return 1;
    }

    char* pinname = argv[1];
    int mode = atoi(argv[2]);

    if (strcmp(pinname, "data") == 0) {
        if (mode == 0) {
            ieee_set_data_input();
        } else {
            ieee_set_data_output();
        }
    }
    else if (strcmp(pinname, "nrfd") == 0) {
        if (mode == 0) {
            set_nrfd_input();
        } else {
            set_nrfd_output();
        }
    }
    else if (strcmp(pinname, "eoi") == 0) {
        if (mode == 0) {
            set_eoi_input();
        } else {
            set_eoi_output();
        }
    }
    else if (strcmp(pinname, "dav") == 0) {
        if (mode == 0) {
            set_dav_input();
        } else {
            set_dav_output();
        }
    }
    else if (strcmp(pinname, "ndac") == 0) {
        if (mode == 0) {
            set_ndac_input();
        } else {
            set_ndac_output();
        }

    }
    return 0;
}

static int pdps(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: pdps <pin_name> <val>\n");
        return 1;
    }

    char* pinname = argv[1];
    int val = atoi(argv[2]);

    if (strcmp(pinname, "data") == 0) {
        uint8_t byte = (uint8_t)val;
        ieee_write_data_byte(byte);
    }
    else if (strcmp(pinname, "nrfd") == 0) {
        if (val == 0) {
            lower_nrfd();
        } else {
            raise_nrfd();
        }
    }
    else if (strcmp(pinname, "eoi") == 0) {
        if (val == 0) {
            lower_eoi();
        } else {
            raise_eoi();
        }
    }
    else if (strcmp(pinname, "dav") == 0) {
        if (val == 0) {
            lower_dav();
        } else {
            raise_dav();
        }
    }
    else if (strcmp(pinname, "ndac") == 0) {
        if (val == 0) {
            lower_ndac();
        } else {
            raise_ndac();
        }
    }

    return 0;
}

static int tog(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: tog <pin>\n");
        return 1;
    }

    char* pinname = argv[1];
    int64_t start = 0;
    int64_t end = 0;

    if (strcmp(pinname, "data") == 0) {
        start = esp_timer_get_time();
        ieee_set_data_output();
        ieee_set_data_input();
        end = esp_timer_get_time();
    }
    else if (strcmp(pinname, "nrfd") == 0) {
        start = esp_timer_get_time();
        set_nrfd_output();
        set_nrfd_input();
        end = esp_timer_get_time();
    }
    else if (strcmp(pinname, "eoi") == 0) {
        start = esp_timer_get_time();
        set_eoi_output();
        set_eoi_input();
        end = esp_timer_get_time();
    }
    else if (strcmp(pinname, "dav") == 0) {
        start = esp_timer_get_time();
        set_dav_output();
        set_dav_input();
        end = esp_timer_get_time();
    }
    else if (strcmp(pinname, "ndac") == 0) {
        start = esp_timer_get_time();
        set_ndac_output();
        set_ndac_input();
        end = esp_timer_get_time();
    }

    int64_t dur = end-start;
    printf("tog %s %" PRIi64 "\n", pinname, dur);
    return 0;
}

static int togmode(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: togmode <pin>\n");
        return 1;
    }

    char* pinname = argv[1];
    int64_t start = 0;
    int64_t end = 0;

    if (strcmp(pinname, "data") == 0) {
        start = esp_timer_get_time();
        uint8_t byte = 0xff;
        ieee_write_data_byte(byte);
        byte = 0x00;
        ieee_write_data_byte(byte);
        end = esp_timer_get_time();
    }
    else if (strcmp(pinname, "nrfd") == 0) {
        start = esp_timer_get_time();
        raise_nrfd();
        lower_nrfd();
        end = esp_timer_get_time();
    }
    else if (strcmp(pinname, "eoi") == 0) {
        start = esp_timer_get_time();
        raise_eoi();
        lower_eoi();
        end = esp_timer_get_time();
    }
    else if (strcmp(pinname, "dav") == 0) {
        start = esp_timer_get_time();
        raise_dav();
        lower_dav();
        end = esp_timer_get_time();
    }
    else if (strcmp(pinname, "ndac") == 0) {
        start = esp_timer_get_time();
        raise_ndac();
        lower_ndac();
        end = esp_timer_get_time();
    }

    int64_t dur = end-start;
    printf("togmode %s %" PRIi64 "\n", pinname, dur);
    return 0;
}

static int rdtest(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: rdtest <pin>\n");
        return 1;
    }

    char* pinname = argv[1];
    int64_t start = 0;
    int64_t end = 0;
    uint8_t byte = 0;

    if (strcmp(pinname, "data") == 0) {
        start = esp_timer_get_time();
        ieee_read_data_byte(byte);
        end = esp_timer_get_time();
    }
    else if (strcmp(pinname, "nrfd") == 0) {
        start = esp_timer_get_time();
        byte = read_nrfd();
        end = esp_timer_get_time();
    }
    else if (strcmp(pinname, "eoi") == 0) {
        start = esp_timer_get_time();
        byte = read_eoi();
        end = esp_timer_get_time();
    }
    else if (strcmp(pinname, "dav") == 0) {
        start = esp_timer_get_time();
        byte = read_dav();
        end = esp_timer_get_time();
    }
    else if (strcmp(pinname, "ndac") == 0) {
        start = esp_timer_get_time();
        byte = read_ndac();
        end = esp_timer_get_time();
    }

    int64_t dur = end-start;
    printf("rdtest %s %" PRIi64 "\n", pinname, dur);
    return 0;
}

static int testpin(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: testpin <test_pin_name> <gpio_in> [datapin]\n");
        return 1;
    }

    char* pinname = argv[1];
    int gpio_in = atoi(argv[2]);

    gpio_set_direction((gpio_num_t)gpio_in, GPIO_MODE_INPUT);

    if (strcmp(pinname, "nrfd") == 0) {
        set_nrfd_output();
        for (int i = 0; i < 4; i++) {
            lower_nrfd();
            hDelayMs(100);
            printf("%d low: %d\n", i, gpio_get_level((gpio_num_t)gpio_in)); 
            raise_nrfd();
            hDelayMs(100);
            printf("%d high: %d\n", i, gpio_get_level((gpio_num_t)gpio_in));
        }
        set_nrfd_input();
    }
    else if (strcmp(pinname, "eoi") == 0) {
        set_eoi_output();
        for (int i = 0; i < 4; i++) {
            lower_eoi();
            hDelayMs(100);
            printf("%d low: %d\n", i, gpio_get_level((gpio_num_t)gpio_in)); 
            raise_eoi();
            hDelayMs(100);
            printf("%d high: %d\n", i, gpio_get_level((gpio_num_t)gpio_in));
        }
        set_eoi_input();
    }
    else if (strcmp(pinname, "dav") == 0) {
        set_dav_output();
        for (int i = 0; i < 4; i++) {
            lower_dav();
            hDelayMs(100);
            printf("%d low: %d\n", i, gpio_get_level((gpio_num_t)gpio_in)); 
            raise_dav();
            hDelayMs(100);
            printf("%d high: %d\n", i, gpio_get_level((gpio_num_t)gpio_in));
        }
        set_dav_input();
    }
    else if (strcmp(pinname, "ndac") == 0) {
        set_ndac_output();
        for (int i = 0; i < 4; i++) {
            lower_ndac();
            hDelayMs(100);
            printf("%d low: %d\n", i, gpio_get_level((gpio_num_t)gpio_in)); 
            raise_ndac();
            hDelayMs(100);
            printf("%d high: %d\n", i, gpio_get_level((gpio_num_t)gpio_in));
        }
        set_ndac_input();
    }
    else if (strcmp(pinname, "data") == 0) {
        int datapin = atoi(argv[3]);
        uint8_t bytehi = 0x1 << datapin;
        uint8_t bytelo = ~bytehi;
        set_datadir_output();
        ieee_set_data_output();
        for (int i = 0; i < 4; i++) {
            ieee_write_data_byte(bytelo);
            hDelayMs(100);
            printf("%d low: %d\n", i, gpio_get_level((gpio_num_t)gpio_in)); 
            ieee_write_data_byte(bytehi);
            hDelayMs(100);
            printf("%d high: %d\n", i, gpio_get_level((gpio_num_t)gpio_in));
        }
        ieee_set_data_input();
    }
    else if (strcmp(pinname, "datadir") == 0) {
        set_datadir_output();
        for (int i = 0; i < 4; i++) {
            lower_datadir();
            hDelayMs(100);
            printf("%d low: %d\n", i, gpio_get_level((gpio_num_t)gpio_in)); 
            raise_datadir();
            hDelayMs(100);
            printf("%d high: %d\n", i, gpio_get_level((gpio_num_t)gpio_in));
        }
    }
    return 0;
}

int pinspeed(int argc, char** argv) {
    int pin = 1;

    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
    printf("gpio speed test for pin: %d\n", pin);

    int i;
    int64_t time_start, time_end;
    double avg_time_us;

    time_start = get_time_us();
    i = fast_gpio_get(pin);
    i = fast_gpio_get(pin);
    i = fast_gpio_get(pin);
    i = fast_gpio_get(pin);
    i = fast_gpio_get(pin);
    i = fast_gpio_get(pin);
    i = fast_gpio_get(pin);
    i = fast_gpio_get(pin);
    i = fast_gpio_get(pin);
    i = fast_gpio_get(pin);
    time_end = get_time_us();
    avg_time_us = (double)(time_end-time_start) / 10.0;
    printf("fast_gpio_get: %" PRIi64 " %lf us\n", time_end-time_start, avg_time_us);
    hDelayMs(250);

    time_start = get_time_us();
    i = EspFastGpio::get(pin);
    i = EspFastGpio::get(pin);
    i = EspFastGpio::get(pin);
    i = EspFastGpio::get(pin);
    i = EspFastGpio::get(pin);
    i = EspFastGpio::get(pin);
    i = EspFastGpio::get(pin);
    i = EspFastGpio::get(pin);
    i = EspFastGpio::get(pin);
    i = EspFastGpio::get(pin);
    time_end = get_time_us();
    avg_time_us = (double)(time_end-time_start) / 10.0;
    printf("EspFastGpio::get: %" PRIi64 " %lf us\n", time_end-time_start, avg_time_us);
    hDelayMs(250);

    time_start = get_time_us();
    i = gpio_get_level((gpio_num_t)pin);
    i = gpio_get_level((gpio_num_t)pin);
    i = gpio_get_level((gpio_num_t)pin);
    i = gpio_get_level((gpio_num_t)pin);
    i = gpio_get_level((gpio_num_t)pin);
    i = gpio_get_level((gpio_num_t)pin);
    i = gpio_get_level((gpio_num_t)pin);
    i = gpio_get_level((gpio_num_t)pin);
    i = gpio_get_level((gpio_num_t)pin);
    i = gpio_get_level((gpio_num_t)pin);
    time_end = get_time_us();
    avg_time_us = (double)(time_end-time_start) / 10.0;
    printf("gpio_get_level: %" PRIi64 " %lf us\n", time_end-time_start, avg_time_us);
    hDelayMs(250);

    // == output
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);

    time_start = get_time_us();
    gpio_set_level((gpio_num_t)pin, 1);
    gpio_set_level((gpio_num_t)pin, 0);
    gpio_set_level((gpio_num_t)pin, 1);
    gpio_set_level((gpio_num_t)pin, 0);
    gpio_set_level((gpio_num_t)pin, 1);
    gpio_set_level((gpio_num_t)pin, 0);
    gpio_set_level((gpio_num_t)pin, 1);
    gpio_set_level((gpio_num_t)pin, 0);
    gpio_set_level((gpio_num_t)pin, 1);
    gpio_set_level((gpio_num_t)pin, 0);
    time_end = get_time_us();
    avg_time_us = (double)(time_end-time_start) / 10.0;
    printf("gpio_set_level: %" PRIi64 " %lf us\n", time_end-time_start, avg_time_us);
    hDelayMs(250);

    time_start = get_time_us();
    EspFastGpio::setHigh(pin);
    EspFastGpio::setLow(pin);
    EspFastGpio::setHigh(pin);
    EspFastGpio::setLow(pin);
    EspFastGpio::setHigh(pin);
    EspFastGpio::setLow(pin);
    EspFastGpio::setHigh(pin);
    EspFastGpio::setLow(pin);
    EspFastGpio::setHigh(pin);
    EspFastGpio::setLow(pin);
    time_end = get_time_us();
    avg_time_us = (double)(time_end-time_start) / 10.0;
    printf("EspFastGpio::set: %" PRIi64 " %lf us\n", time_end-time_start, avg_time_us);
    hDelayMs(250);

    // now set output
    time_start = get_time_us();
    fast_gpio_set_high(pin);
    fast_gpio_set_low(pin);
    fast_gpio_set_high(pin);
    fast_gpio_set_low(pin);
    fast_gpio_set_high(pin);
    fast_gpio_set_low(pin);
    fast_gpio_set_high(pin);
    fast_gpio_set_low(pin);
    fast_gpio_set_high(pin);
    fast_gpio_set_low(pin);
    time_end = get_time_us();
    avg_time_us = (double)(time_end-time_start) / 10.0;
    printf("EspFastGpio::set: %" PRIi64 " %lf us\n", time_end-time_start, avg_time_us);
    hDelayMs(250);

    printf("direction test\n");
    time_start = get_time_us();
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
    time_end = get_time_us();
    avg_time_us = (double)(time_end-time_start) / 10.0;
    printf("gpio_set_direction: %" PRIi64 " %lf us\n", time_end-time_start, avg_time_us);
    hDelayMs(250);

    time_start = get_time_us();
    EspFastGpio::setOutput(pin);
    EspFastGpio::setInput(pin);
    EspFastGpio::setOutput(pin);
    EspFastGpio::setInput(pin);
    EspFastGpio::setOutput(pin);
    EspFastGpio::setInput(pin);
    EspFastGpio::setOutput(pin);
    EspFastGpio::setInput(pin);
    EspFastGpio::setOutput(pin);
    EspFastGpio::setInput(pin);
    time_end = get_time_us();
    avg_time_us = (double)(time_end-time_start) / 10.0;
    printf("fast gpio set i/o: %" PRIi64 " %lf us\n", time_end-time_start, avg_time_us);
    hDelayMs(250);

    time_start = get_time_us();
    fast_gpio_set_output(pin);
    fast_gpio_set_input(pin);
    fast_gpio_set_output(pin);
    fast_gpio_set_input(pin);
    fast_gpio_set_output(pin);
    fast_gpio_set_input(pin);
    fast_gpio_set_output(pin);
    fast_gpio_set_input(pin);
    fast_gpio_set_output(pin);
    fast_gpio_set_input(pin);
    time_end = get_time_us();
    avg_time_us = (double)(time_end-time_start) / 10.0;
    printf("mac set i/o: %" PRIi64 " %lf us\n", time_end-time_start, avg_time_us);

    return 0;
}

void hardware_cmd_init() {
    Console::add_command("ledinit", NULL, ledinit);
    Console::add_command("ledset", NULL, ledset);
    Console::add_command("pdpm", NULL, pdpm);
    Console::add_command("pdps", NULL, pdps);
    Console::add_command("tog", NULL, tog);
    Console::add_command("togmode", NULL, togmode);
    Console::add_command("rdtest", NULL, rdtest);
    Console::add_command("testpin", NULL, testpin);
    Console::add_command("pinspeed", NULL, pinspeed);
}

void hDelayMs(int ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

uint8_t bf_pgm_read_byte(uint8_t* src)
{
    return *src;
}

void bf_eeprom_write_block(const void* block, void* eeprom, size_t n)
{
    int index = (int)eeprom;
    memcpy(&eeprom_data[index], block, n);

    esp_err_t err = nvs_set_blob(eeprom_nvs_handle, "eeprom", eeprom_data, 512);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to write eeprom");
        return;
    }

    err = nvs_commit(eeprom_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to commit eeprom value");
    }
}

void bf_eeprom_read_block(void* block, const void* eeprom, size_t n)
{
    int index = (int)eeprom;
    memcpy(block, &eeprom_data[index], n);
}

uint8_t bf_eeprom_read_byte(const uint8_t* addr)
{
    int index = (int)addr;
    return eeprom_data[index];
}

void spi_init() {
    ESP_LOGI("spi", "spi init");
    setPinMode(CS_PIN, OUTPUT);
    spi_cs_unselect();

    setPinMode(MISO_PIN, INPUT_PULLUP);
    setPinMode(MOSI_PIN, OUTPUT);
    setPinMode(SCK_PIN, OUTPUT);

    fast_gpio_write(MOSI_PIN, LOW);
    fast_gpio_write(SCK_PIN, LOW);
}

static void delay_cycles(uint32_t cycles) {
    uint32_t start = esp_cpu_get_cycle_count();
    while ((esp_cpu_get_cycle_count() - start) < cycles) {
        __asm__ __volatile__("nop");
    }
}

uint8_t spi_transmit(uint8_t data) {
    uint8_t recv = 0;
    uint8_t bit = 0;
    uint32_t cycleDelay = 5;
    fast_gpio_write(SCK_PIN, LOW);
    for (int i = 7; i >= 0; i--) {
        // get current output bit
        bit = (data >> i) & 0x1;
        fast_gpio_write(SCK_PIN, LOW);
        fast_gpio_write(MOSI_PIN, bit);
        // wait
        delay_cycles(cycleDelay);
        // raise sck
        fast_gpio_write(SCK_PIN, HIGH);

        // sample miso
        bit = read_miso();
        recv <<= 1;
        recv += bit;
        delay_cycles(cycleDelay);
    }
    fast_gpio_write(MOSI_PIN, LOW);
    fast_gpio_write(SCK_PIN, LOW);
    return recv;
}

void spi_cs_select() {
    fast_gpio_write(CS_PIN, LOW);
}

void spi_cs_unselect() {
    fast_gpio_write(CS_PIN, HIGH);
}

bool isFirmwareFile(char* fname)
{
    if (fname == NULL)
    {
        return false;
    }

    if (strlen(fname) != 12)
    {
        return false;
    }

    if (fname[0] == 'F' && 
        fname[1] == 'I' && 
        fname[2] == 'R' && 
        fname[3] == 'M' &&
        fname[9] == 'P' &&
        fname[10] == 'D' &&
#ifdef CONFIG_IDF_TARGET_ESP32
        fname[11] == '2')
#else
        fname[11] == '3')
#endif
    {
        return true;
    }

    return false;
}

int32_t nvs_get_int(const char* key) {
    int32_t val = -1;
    if (nvs_get_i32(eeprom_nvs_handle, key, &val) != ESP_OK) {
        return -1;
    }

    return val;
}

void nvs_set_int(const char* key, int32_t val) {
    esp_err_t err = nvs_set_i32(eeprom_nvs_handle, key, val);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_int, failed to set %s -> %" PRIi32, key, val);
        return;
    }

    err = nvs_commit(eeprom_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to commit int value");
    }
}

extern TaskHandle_t loopTaskHandle;

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    disable_interrupts();
    set_ndac_output();
    lower_ndac();
    BaseType_t xHigherPriorityTaskWoken;
    vTaskNotifyGiveFromISR(loopTaskHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void setup_atn_interrupt() {
    // set up ATN as interrupt
    esp_err_t err = gpio_set_intr_type((gpio_num_t)ATN_PIN, GPIO_INTR_NEGEDGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to set ATN interrupt");
    }
    gpio_install_isr_service(0);
    gpio_isr_handler_add((gpio_num_t)ATN_PIN, gpio_isr_handler, NULL);
}

void wait_atn_isr() {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

void clear_atn() {
    enable_interrupts();
    set_led(true);
}