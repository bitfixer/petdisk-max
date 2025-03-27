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
#include <nvs.h>
//#include <Arduino.h>
//#include <EEPROM.h>
//#include <SPI.h>

#define TAG "hardware"

#define LED_PIN     2
#define CS_PIN      4

#define MISO_PIN   19
#define MOSI_PIN   23
#define SCK_PIN    18

gpio_hal_context_t _gpio_hal = {
    .dev = GPIO_HAL_GET_HW(GPIO_PORT_0)
};

gpio_dev_t* dev = GPIO_HAL_GET_HW(GPIO_PORT_0);

volatile uint32_t* gpio_low_set_reg = &(dev->out_w1ts);
volatile uint32_t* gpio_low_clear_reg = &(dev->out_w1tc);
volatile uint32_t* gpio_low_enable_set_reg = &(dev->enable_w1ts);
volatile uint32_t* gpio_low_enable_clear_reg = &(dev->enable_w1tc);

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
    // nothing for esp32
    setOutput(LED_PIN);
}

void set_led(bool value)
{
    if (value == true)
    {
        digitalWrite2(LED_PIN, HIGH);
    }
    else
    {
        digitalWrite2(LED_PIN, LOW);
    }
}

void hDelayMs(int ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

int getMs() {
    return (int)(xTaskGetTickCount() * portTICK_PERIOD_MS);
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

    digitalWrite2(MOSI_PIN, LOW);
    digitalWrite2(SCK_PIN, LOW);
}

uint8_t spi_transmit(uint8_t data) {
    uint8_t recv = 0;
    uint8_t bit = 0;
    uint32_t clockDelay = 2;
    digitalWrite2(SCK_PIN, LOW);
    for (int i = 7; i >= 0; i--) {
        // get current output bit
        bit = (data >> i) & 0x1;
        digitalWrite2(SCK_PIN, LOW);
        digitalWrite2(MOSI_PIN, bit);
        // wait
        esp_rom_delay_us(clockDelay);
        // raise sck
        digitalWrite2(SCK_PIN, HIGH);

        // sample miso
        bit = digitalRead2(MISO_PIN);
        recv <<= 1;
        recv += bit;
        esp_rom_delay_us(clockDelay);
    }
    digitalWrite2(MOSI_PIN, LOW);
    digitalWrite2(SCK_PIN, LOW);
    return recv;
}

void spi_cs_select() {
    digitalWrite2(CS_PIN, LOW);
}

void spi_cs_unselect() {
    digitalWrite2(CS_PIN, HIGH);
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
        fname[11] == '2')
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