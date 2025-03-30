#include "console.h"
#include <string.h>
#include <esp_console.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
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

#define MIN(a,b) (a<b?a:b)
#define MAX(a,b) (a>b?a:b)

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // Clean the buffer in case of a new request
            if (output_len == 0 && evt->user_data) {
                // we are just starting to copy the output data into the use
                memset(evt->user_data, 0, 1024);
            }
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                int copy_len = 0;
                if (evt->user_data) {
                    // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
                    copy_len = MIN(evt->data_len, (1024 - output_len));
                    if (copy_len) {
                        memcpy(evt->user_data + output_len, evt->data, copy_len);
                    }
                } else {
                    int content_len = esp_http_client_get_content_length(evt->client);
                    if (output_buffer == NULL) {
                        // We initialize output_buffer with 0 because it is used by strlen() and similar functions therefore should be null terminated.
                        output_buffer = (char *) calloc(content_len + 1, sizeof(char));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    copy_len = MIN(evt->data_len, (content_len - output_len));
                    if (copy_len) {
                        memcpy(output_buffer + output_len, evt->data, copy_len);
                    }
                }
                output_len += copy_len;
            }
            break;
        default:
            break;
    }
    return ESP_OK;
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

/*
int http(int argc, char** argv) {
    printf("connecting to wifi\n");
    bitfixer::EspConn conn;
    char* ssid = argv[1];
    char* password = argv[2];
    conn.connect(ssid, password);

    printf("waiting for connect.\n");
    //while (!conn.isConnected()) {
    //    hDelayMs(1000);
    //    printf(".\n");
    //}

    char local_response_buffer[1024] = {0};
    esp_http_client_config_t config;
    memset(&config, 0, sizeof(esp_http_client_config_t));
    //config.host = "bitfixer.com";
    //config.path = "/pd/petdisk.php";
    //config.query = "?d=1";
    config.url = "http://bitfixer.com/pd/petdisk.php?d=1";
    config.event_handler = _http_event_handler;
    config.user_data = local_response_buffer;        // Pass address of local buffer to get response
    config.disable_auto_redirect = true;
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // GET
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    ESP_LOG_BUFFER_HEX(TAG, local_response_buffer, strlen(local_response_buffer));
    int len = esp_http_client_get_content_length(client);
    for (int i = 0; i < len; i++) {
        ESP_LOGI(TAG, "resp %d: %X %c", i, local_response_buffer[i], local_response_buffer[i]);
    }
    //ESP_LOGI(TAG, "got buffer: %s", local_response_buffer);
    return 0;
}
*/

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
    add_command("mem", NULL, mem);
    add_command("http", "http", http);
    add_command("wificonn", NULL, wificonn);
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