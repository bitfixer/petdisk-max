#include "EspConn.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <esp_wifi.h>
#include <nvs_flash.h>
#include <esp_ipc.h>

#include "hardware.h"
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <lwip/dns.h>

namespace bitfixer 
{

static bool _connected = false;

bool EspConn::isConnected() {
    return _connected;
}

bool EspConn::initWithParams(uint8_t* buffer, uint16_t* bufferSize)
{
    _serialBuffer = buffer;
    _serialBufferSize = bufferSize;
    return true;
}

static esp_netif_t *s_example_sta_netif = NULL;
static SemaphoreHandle_t s_semph_get_ip_addrs = NULL;
static int s_retry_num = 0;

static void example_handler_on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    _connected = false;
    s_retry_num++;
    if (s_retry_num > 5) {
        log_i("WiFi Connect failed %d times, stop reconnect.", s_retry_num);
        /* let example_wifi_sta_do_connect() return */
        if (s_semph_get_ip_addrs) {
            xSemaphoreGive(s_semph_get_ip_addrs);
        }
        return;
    }
    log_i("Wi-Fi disconnected, trying to reconnect...");
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_STARTED) {
        return;
    }
    ESP_ERROR_CHECK(err);
}

static void example_handler_on_wifi_connect(void *esp_netif, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    log_i("connect");
}

static void example_handler_on_sta_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    s_retry_num = 0;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    //if (!example_is_our_netif(EXAMPLE_NETIF_DESC_STA, event->esp_netif)) {
    //    return;
    //}
    log_i("Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
    if (s_semph_get_ip_addrs) {
        xSemaphoreGive(s_semph_get_ip_addrs);
    } else {
        log_i("- IPv4 address: " IPSTR ",", IP2STR(&event->ip_info.ip));
    }

    _connected = true;
}

esp_err_t example_wifi_sta_do_connect(wifi_config_t wifi_config, bool wait)
{
    if (wait) {
        s_semph_get_ip_addrs = xSemaphoreCreateBinary();
        if (s_semph_get_ip_addrs == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    s_retry_num = 0;
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &example_handler_on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &example_handler_on_sta_got_ip, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &example_handler_on_wifi_connect, s_example_sta_netif));

    log_i("Connecting to %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        log_e("WiFi connect failed! ret:%x", ret);
        return ret;
    }
    if (wait) {
        log_i("Waiting for IP(s)");
        xSemaphoreTake(s_semph_get_ip_addrs, portMAX_DELAY);
        if (s_retry_num > 5) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}


bool EspConn::connect(const char* ssid, const char* passphrase) {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    s_example_sta_netif = esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));
    strcpy((char*)wifi_config.sta.ssid, ssid);
    strcpy((char*)wifi_config.sta.password, passphrase);
    wifi_config.sta.scan_method = WIFI_FAST_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    example_wifi_sta_do_connect(wifi_config, true);
    return true;
}

static bool dnsFound = false;
static ip_addr_t ipAddr;

static void dns_found_cb(const char *name, const ip_addr_t* ipaddr, void* callback_arg)
{
    ipAddr = *ipaddr;
    dnsFound = true;
}

// check if a string is an ip address already
static bool isIP(const char* host) {
    int f[4];
    int res = sscanf(host, "%d.%d.%d.%d", &f[0], &f[1], &f[2], &f[3]);
    if (res != 4) {
        return false;
    }

    return true;
}

bool EspConn::startClient(const char* host, uint16_t port)
{
    // TODO: check if connection is actually made
    _port = port;
    if (isIP(host)) {
        strcpy(_host, host);
    } else {
        dns_gethostbyname(host, &ipAddr, dns_found_cb, NULL);

        while (!dnsFound) {
            hDelayMs(100);
        }

        sprintf(_host, "%i.%i.%i.%i", 
            ip4_addr1(&ipAddr.u_addr.ip4), 
            ip4_addr2(&ipAddr.u_addr.ip4), 
            ip4_addr3(&ipAddr.u_addr.ip4), 
            ip4_addr4(&ipAddr.u_addr.ip4));
    }
    return true;
}

typedef struct _httpArgs {
    char* host;
    int port;
    uint8_t* sendData;
    int dataLen;
    uint8_t* recvData;
    int recCount;
} httpArgs;

static void http_fetch(void* args) {
    httpArgs* hargs = (httpArgs*)args;
    int addr_family = 0;
    int ip_protocol = 0;

    struct sockaddr_in dest_addr;
    inet_pton(AF_INET, hargs->host, &dest_addr.sin_addr);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(hargs->port);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    int sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (sock < 0) {
        return;
    }
    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        return;
    }
    err = send(sock, hargs->sendData, hargs->dataLen, 0);
    if (err < 0) {
        return;
    }

    // keep receiving
    uint8_t* recvbuf = hargs->recvData;
    while (1) {
        int len = recv(sock, recvbuf, 1000, 0);
        // Error occurred during receiving
        if (len < 0) {
            break;
        } else {
            hargs->recCount += len;
            recvbuf += len;
        }
    }

    if (sock != -1) {
        shutdown(sock, 0);
        close(sock);
    }
}

void EspConn::sendData(uint8_t sock, unsigned char* data, int len)
{
    httpArgs args;
    args.host = _host;
    args.port = _port;
    args.sendData = data;
    args.dataLen = len;
    args.recvData = _serialBuffer;
    args.recCount = 0;

    //enable_interrupts();
#if CONFIG_IDF_TARGET_ESP32
    esp_ipc_call_blocking(0, http_fetch, (void*)&args);
#else
    http_fetch(&args);
#endif
    //disable_interrupts();
    
    *_serialBufferSize = args.recCount;
}

}