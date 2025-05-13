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

#include "console.h"
#include "IEEE488.h"

//#include <Arduino.h>
//#include <EEPROM.h>
//#include <SPI.h>
static const char* TAG = "hw";

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
        setOutput(DATADIR);
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

void hardware_cmd_init() {
    Console::add_command("ledinit", NULL, ledinit);
    Console::add_command("ledset", NULL, ledset);
    Console::add_command("pdpm", NULL, pdpm);
    Console::add_command("pdps", NULL, pdps);
    Console::add_command("tog", NULL, tog);
    Console::add_command("togmode", NULL, togmode);
    Console::add_command("rdtest", NULL, rdtest);
    Console::add_command("testpin", NULL, testpin);
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

static QueueHandle_t atn_queue = NULL;
static bool b;
static bitfixer::IEEE488* _ieee = bitfixer::IEEE488::get_instance();

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    set_ndac_output();
    lower_ndac();
    // wait for dav low
    xQueueSendFromISR(atn_queue, (void*)&b, NULL);
}

void setup_atn_interrupt() {
    atn_queue = xQueueCreate(1, sizeof(bool));
    // set up ATN as interrupt
    esp_err_t err = gpio_set_intr_type((gpio_num_t)ATN_PIN, GPIO_INTR_NEGEDGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to set ATN interrupt");
    }
    gpio_install_isr_service(0);
    gpio_isr_handler_add((gpio_num_t)ATN_PIN, gpio_isr_handler, NULL);
}

void wait_atn_isr() {
    bool t;
    xQueueReceive(atn_queue, &t, portMAX_DELAY);
}

void clear_atn() {
}