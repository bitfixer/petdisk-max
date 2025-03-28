#include "console.h"
#include <string.h>
#include <esp_console.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <driver/gpio.h>

namespace Console {

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

static void _init_command(const char* cmd, const char* help, esp_console_cmd_func_t cmdfunc) {
    char* cptr = (char*)heap_caps_malloc(strlen(cmd)+1, MALLOC_CAP_INTERNAL);
    strcpy(cptr, cmd);

    char *chelp = NULL;
    if (help) {
      chelp = (char*)heap_caps_malloc(strlen(help)+1, MALLOC_CAP_INTERNAL);
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
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    _init_commands();
}



}