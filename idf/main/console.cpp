#include "console.h"
#include <string.h>
#include <esp_console.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_random.h>
#include <driver/gpio.h>
#include <esp_http_client.h>
#include "EspConn.h"
#include "HTTPDataSource.h"
#include "hardware.h"
#include "http.h"

namespace Console {

static const char* TAG = "cons";
static const char* _hint = "";

int reset(int argc, char** argv) {
    printf("RESET\n");
    esp_restart();
}

int gpioset(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: gpioset <pin> <val>\n");
        return 1;
    }

    int pin = atoi(argv[1]);
    int val = atoi(argv[2]);

    gpio_set_level((gpio_num_t)pin, (uint32_t)val);
    return 0;
}

int gpiomode(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: gpiomode <pin> <dir>\n");
        return 1;
    }

    int pin = atoi(argv[1]);
    int dir = atoi(argv[2]);
    printf("set pin %d dir %d\n", pin, dir);

    if (dir == 0) {
        gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
    } else {
        gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
    }
    return 0;
}

int mem(int argc, char** argv) {
    size_t freemem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t spimem = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    printf("free heap: internal %d spiram %d\n", (int)freemem, (int)spimem);
    return 0;
}

int wificonn(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: wificonn <ssid> <password>\n");
        return 1;
    }

    char* ssid = argv[1];
    char* password = argv[2];

    bitfixer::EspConn conn;
    conn.connect(ssid, password);
    return 0;
}

int http(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: http <url>\n");
        return 1;
    }

    uint8_t* buf = (uint8_t*)heap_caps_malloc(64*1024, MALLOC_CAP_SPIRAM);
    bool res = HTTP::request("http://bitfixer.com/pd/petdisk.php?d=1", buf, 64*1024);
    printf("result: %d\n", (int)res);
    if (res) {
        printf("result: %s\n", (char*)buf);
    }

    heap_caps_free(buf);
    return 0;
}

static int httpds(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: httpds < 2\n");
        return 1;
    }

    char* fname = argv[1];
    printf("requesting: %s\n", fname);
    bitfixer::HTTPDataSource ds;
    bool res = ds.openFileForReading((uint8_t*)fname);

    printf("read result: %d\n", (int)res);
    if (res <= 0) {
        printf("test failed\n");
        return 0;
    }

    int block_count = 0;
    int total_size = 0;
    // check blocks in datasource
    do {
        uint16_t block_size = ds.getNextFileBlock();
        block_count++;
        total_size += (int)block_size;
        printf("block %d size %d total %d\n", block_count, block_size, total_size);
    } while (!ds.isLastBlock());

    return 0;
}

static int psramtest(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: psramtest <bufsize>\n");
        return 1;
    }

    // perform test on available PSRAM
    int test_chunk_size = atoi(argv[1]);

    printf("performing psram test with chunk size: %d\n", test_chunk_size);

    // allocate internal buffer for comparison
    uint8_t* ctrl = (uint8_t*)heap_caps_malloc(test_chunk_size, MALLOC_CAP_INTERNAL);
    if (!ctrl) {
        printf("failed to allocate control buffer!\n");
        return 0;
    }
    // fill with random bytes
    esp_fill_random(ctrl, test_chunk_size);

    // allocate chunks from psram until we can't anymore
    uint8_t* psram_chunks[100];
    for (int i = 0; i < 100; i++) {
        psram_chunks[i] = NULL;
    }

    // now keep allocating chunks
    for (int i = 0; i < 100; i++) {
        uint8_t* chunk = (uint8_t*)heap_caps_malloc(test_chunk_size, MALLOC_CAP_SPIRAM);
        if (!chunk) {
            printf("malloc failed at chunk %d\n", i);
            break;
        }

        psram_chunks[i] = chunk;
        memset(chunk, 0, test_chunk_size);

        // verify internal chunk does not match
        if (memcmp(ctrl, chunk, test_chunk_size) == 0) {
            printf("failed at %d, chunk should not match control\n", i);
            break;
        }

        // now copy from control
        memcpy(chunk, ctrl, test_chunk_size);

        // verify
        if (memcmp(ctrl, chunk, test_chunk_size) != 0) {
            printf("failed at %d, chunk does not match control\n", i);
            break;
        }

        printf("chunk %d: pass\n", i);
        size_t freemem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t spimem = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        printf("free heap: internal %d spiram %d\n", (int)freemem, (int)spimem);
    }

    printf("freeing chunks\n");

    for (int i = 0; i < 100; i++) {
        if (psram_chunks[i] == NULL) {
            printf("done freeing chunks at %d\n", i);
            break;
        }

        heap_caps_free(psram_chunks[i]);
    }

    size_t freemem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t spimem = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    printf("free heap: internal %d spiram %d\n", (int)freemem, (int)spimem);

    printf("done\n");
    return 0;
}

static void _init_command(const char* cmd, const char* help, esp_console_cmd_func_t cmdfunc) {
    char* cptr = (char*)malloc(strlen(cmd)+1);
    strcpy(cptr, cmd);

    char *chelp = NULL;
    if (help) {
      chelp = (char*)malloc(strlen(help)+1);
      strcpy(chelp, help);
    }

    esp_console_cmd_t c = {
        .command = cptr,
        .help = chelp,
        .hint = _hint,
        .func = cmdfunc,
        .argtable = NULL,
        .func_w_context = NULL,
        .context = NULL
    };
    esp_console_cmd_register(&c);
}

void add_command(const char* cmd, const char* help, int(*func)(int,char**)) {
    _init_command(cmd, help, func);
}

static void _init_commands() {
    add_command("reset", "reset", reset);
    add_command("gpiomode", "gpiomode", gpiomode);
    add_command("gpioset", "gpioset", gpioset);
    add_command("mem", NULL, mem);
    add_command("http", "http", http);
    add_command("wificonn", NULL, wificonn);
    add_command("httpds", NULL, httpds);
    add_command("psramtest", NULL, psramtest);
}

void init() {
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = "petdisk>";
    repl_config.max_cmdline_length = 80;

    esp_console_register_help_command();
#if CONFIG_ESP_CONSOLE_USB_CDC
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));
#else
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
#endif
    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    _init_commands();
}



}