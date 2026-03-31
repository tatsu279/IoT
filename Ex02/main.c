#include <stdio.h>
#include <string.h>

#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_http_server.h"

// ===== WIFI =====
#define WIFI_SSID "Kun"
#define WIFI_PASS "12345678"

// ===== RELAY =====
#define RELAY_GPIO GPIO_NUM_13

// ===== BUTTON =====
#define BUTTON_GPIO GPIO_NUM_14

static const char *TAG = "SMART_RELAY";

bool relay_state = false;

// ================= RELAY CONTROL =================
void set_relay(bool state)
{
    relay_state = state;

    if (state)
    {
        gpio_set_level(RELAY_GPIO, 1); // ON
        ESP_LOGI(TAG, "Relay ON");
    }
    else
    {
        gpio_set_level(RELAY_GPIO, 0); // OFF
        ESP_LOGI(TAG, "Relay OFF");
    }
}

// ================= HTTP =================
esp_err_t on_handler(httpd_req_t *req)
{
    set_relay(true);
    httpd_resp_send(req, "ON", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t off_handler(httpd_req_t *req)
{
    set_relay(false);
    httpd_resp_send(req, "OFF", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// ================= WEB SERVER =================
void start_webserver()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t uri_on = {
            .uri = "/on",
            .method = HTTP_GET,
            .handler = on_handler};

        httpd_uri_t uri_off = {
            .uri = "/off",
            .method = HTTP_GET,
            .handler = off_handler};

        httpd_register_uri_handler(server, &uri_on);
        httpd_register_uri_handler(server, &uri_off);

        ESP_LOGI(TAG, "Webserver started");
    }
}

// ================= BUTTON TASK =================
void button_task(void *arg)
{
    int last_state = 1;

    while (1)
    {
        int current_state = gpio_get_level(BUTTON_GPIO);

        if (last_state == 1 && current_state == 0)
        {
            // nhấn nút
            relay_state = !relay_state;
            set_relay(relay_state);

            vTaskDelay(200 / portTICK_PERIOD_MS); // chống dội
        }

        last_state = current_state;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// ================= WIFI =================
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ESP_LOGI(TAG, "WiFi connected");
        start_webserver();
    }
}

void wifi_init()
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

// ================= MAIN =================
void app_main(void)
{
    nvs_flash_init();

    // Relay
    gpio_reset_pin(RELAY_GPIO);
    gpio_set_direction(RELAY_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY_GPIO, 1);

    // Button
    gpio_reset_pin(BUTTON_GPIO);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);

    // WiFi
    wifi_init();

    // Task đọc nút
    xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);
}