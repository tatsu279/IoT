#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_bt.h>
#include <mqtt_client.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>
#include "qrcode.h"

/* ============================================================
 * CONFIGURATION
 * ============================================================ */
static const char *TAG = "app_main";

#define GPIO_RELAY      GPIO_NUM_27  
#define GPIO_BUTTON     GPIO_NUM_26 

#define MQTT_BROKER_URI "mqtt://broker.emqx.io:1883"

#define CONFIG_EXAMPLE_PROV_MGR_MAX_RETRY_CNT 3
#define EXAMPLE_PROV_SEC2_USERNAME "wifiprov"
#define EXAMPLE_PROV_SEC2_PWD      "abcd1234"

/* GLOBAL VARIABLES */
static bool relay_state = false;
static char g_device_id[32] = {0};
static char topic_status[128];
static char topic_ctrl[128];
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool mqtt_connected = false;

static uint8_t custom_service_uuid[16] = {
    0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
    0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
};

/* Wi-Fi Event Group */
const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;

/* ============================================================
 * PROVISIONING SECURITY SEC2 CONFIGURATION
 * ============================================================ */
static const char sec2_salt[] = {
    0x03, 0x6e, 0xe0, 0xc7, 0xbc, 0xb9, 0xed, 0xa8, 0x4c, 0x9e, 0xac, 0x97, 0xd9, 0x3d, 0xec, 0xf4};
static const char sec2_verifier[] = {
    0x7c, 0x7c, 0x85, 0x47, 0x65, 0x08, 0x94, 0x6d, 0xd6, 0x36, 0xaf, 0x37, 0xd7, 0xe8, 0x91, 0x43,
    0x78, 0xcf, 0xfd, 0x61, 0x6c, 0x59, 0xd2, 0xf8, 0x39, 0x08, 0x12, 0x72, 0x38, 0xde, 0x9e, 0x24,
    0xa4, 0x70, 0x26, 0x1c, 0xdf, 0xa9, 0x03, 0xc2, 0xb2, 0x70, 0xe7, 0xb1, 0x32, 0x24, 0xda, 0x11,
    0x1d, 0x97, 0x18, 0xdc, 0x60, 0x72, 0x08, 0xcc, 0x9a, 0xc9, 0x0c, 0x48, 0x27, 0xe2, 0xae, 0x89,
    0xaa, 0x16, 0x25, 0xb8, 0x04, 0xd2, 0x1a, 0x9b, 0x3a, 0x8f, 0x37, 0xf6, 0xe4, 0x3a, 0x71, 0x2e,
    0xe1, 0x27, 0x86, 0x6e, 0xad, 0xce, 0x28, 0xff, 0x54, 0x46, 0x60, 0x1f, 0xb9, 0x96, 0x87, 0xdc,
    0x57, 0x40, 0xa7, 0xd4, 0x6c, 0xc9, 0x77, 0x54, 0xdc, 0x16, 0x82, 0xf0, 0xed, 0x35, 0x6a, 0xc4,
    0x70, 0xad, 0x3d, 0x90, 0xb5, 0x81, 0x94, 0x70, 0xd7, 0xbc, 0x65, 0xb2, 0xd5, 0x18, 0xe0, 0x2e,
    0xc3, 0xa5, 0xf9, 0x68, 0xdd, 0x64, 0x7b, 0xb8, 0xb7, 0x3c, 0x9c, 0xfc, 0x00, 0xd8, 0x71, 0x7e,
    0xb7, 0x9a, 0x7c, 0xb1, 0xb7, 0xc2, 0xc3, 0x18, 0x34, 0x29, 0x32, 0x43, 0x3e, 0x00, 0x99, 0xe9,
    0x82, 0x94, 0xe3, 0xd8, 0x2a, 0xb0, 0x96, 0x29, 0xb7, 0xdf, 0x0e, 0x5f, 0x08, 0x33, 0x40, 0x76,
    0x52, 0x91, 0x32, 0x00, 0x9f, 0x97, 0x2c, 0x89, 0x6c, 0x39, 0x1e, 0xc8, 0x28, 0x05, 0x44, 0x17,
    0x3f, 0x68, 0x02, 0x8a, 0x9f, 0x44, 0x61, 0xd1, 0xf5, 0xa1, 0x7e, 0x5a, 0x70, 0xd2, 0xc7, 0x23,
    0x81, 0xcb, 0x38, 0x68, 0xe4, 0x2c, 0x20, 0xbc, 0x40, 0x57, 0x76, 0x17, 0xbd, 0x08, 0xb8, 0x96,
    0xbc, 0x26, 0xeb, 0x32, 0x46, 0x69, 0x35, 0x05, 0x8c, 0x15, 0x70, 0xd9, 0x1b, 0xe9, 0xbe, 0xcc,
    0xa9, 0x38, 0xa6, 0x67, 0xf0, 0xad, 0x50, 0x13, 0x19, 0x72, 0x64, 0xbf, 0x52, 0xc2, 0x34, 0xe2,
    0x1b, 0x11, 0x79, 0x74, 0x72, 0xbd, 0x34, 0x5b, 0xb1, 0xe2, 0xfd, 0x66, 0x73, 0xfe, 0x71, 0x64,
    0x74, 0xd0, 0x4e, 0xbc, 0x51, 0x24, 0x19, 0x40, 0x87, 0x0e, 0x92, 0x40, 0xe6, 0x21, 0xe7, 0x2d,
    0x4e, 0x37, 0x76, 0x2f, 0x2e, 0xe2, 0x68, 0xc7, 0x89, 0xe8, 0x32, 0x13, 0x42, 0x06, 0x84, 0x84,
    0x53, 0x4a, 0xb3, 0x0c, 0x1b, 0x4c, 0x8d, 0x1c, 0x51, 0x97, 0x19, 0xab, 0xae, 0x77, 0xff, 0xdb,
    0xec, 0xf0, 0x10, 0x95, 0x34, 0x33, 0x6b, 0xcb, 0x3e, 0x84, 0x0f, 0xb9, 0xd8, 0x5f, 0xb8, 0xa0,
    0xb8, 0x55, 0x53, 0x3e, 0x70, 0xf7, 0x18, 0xf5, 0xce, 0x7b, 0x4e, 0xbf, 0x27, 0xce, 0xce, 0xa8,
    0xb3, 0xbe, 0x40, 0xc5, 0xc5, 0x32, 0x29, 0x3e, 0x71, 0x64, 0x9e, 0xde, 0x8c, 0xf6, 0x75, 0xa1,
    0xe6, 0xf6, 0x53, 0xc8, 0x31, 0xa8, 0x78, 0xde, 0x50, 0x40, 0xf7, 0x62, 0xde, 0x36, 0xb2, 0xba};

static esp_err_t example_get_sec2_salt(const char **salt, uint16_t *salt_len) __attribute__((unused));
static esp_err_t example_get_sec2_verifier(const char **verifier, uint16_t *verifier_len) __attribute__((unused));

static esp_err_t example_get_sec2_salt(const char **salt, uint16_t *salt_len)
{
    ESP_LOGD(TAG, "Development mode: using hard coded salt");
    *salt = sec2_salt;
    *salt_len = sizeof(sec2_salt);
    return ESP_OK;
}

static esp_err_t example_get_sec2_verifier(const char **verifier, uint16_t *verifier_len)
{
    ESP_LOGD(TAG, "Development mode: using hard coded verifier");
    *verifier = sec2_verifier;
    *verifier_len = sizeof(sec2_verifier);
    return ESP_OK;
}

/* ============================================================
 * RELAY & BUTTON HANDLERS
 * ============================================================ */
static void relay_set(bool on)
{
    relay_state = on;
    gpio_set_level(GPIO_RELAY, on ? 1 : 0);
    ESP_LOGI(TAG, "RELAY -> %s", on ? "ON" : "OFF");
}

static void mqtt_publish_status(void)
{
    if (!mqtt_connected || !s_mqtt_client) return;
    char buf[160];
    snprintf(buf, sizeof(buf), "{\"device_id\":\"%s\",\"relay\":\"%s\",\"online\":true}",
             g_device_id, relay_state ? "ON" : "OFF");
    esp_mqtt_client_publish(s_mqtt_client, topic_status, buf, 0, 1, 1);
    ESP_LOGI(TAG, "MQTT Pub [%s]: %s", topic_status, buf);
}

/* ============================================================
 * BUTTON HANDLER (TTP223 Digital Input)
 * ============================================================ */

/* GPIO Button Handler - Xử lý chạm nút (TTP223) */
static void button_task(void *arg)
{
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << GPIO_NUM_26),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_cfg);

    vTaskDelay(pdMS_TO_TICKS(100));
    int last = gpio_get_level(GPIO_NUM_26);
    
    while (1) {
        int cur = gpio_get_level(GPIO_NUM_26);
        
        /* Phát hiện Rising Edge (0→1 khi chạm vào) */
        if (cur == 1 && last == 0) {
            vTaskDelay(pdMS_TO_TICKS(50));  /* Chống dội */
            if (gpio_get_level(GPIO_NUM_26) == 1) {
                relay_set(!relay_state);
                mqtt_publish_status();
                ESP_LOGI(TAG, "Chạm phát hiện → Relay %s", relay_state ? "ON" : "OFF");
            }
        }
        
        last = cur;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void hw_init(void)
{
    gpio_config_t rel_cfg = {
        .pin_bit_mask = (1ULL << GPIO_RELAY),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&rel_cfg);
    gpio_set_level(GPIO_RELAY, 0);

    /* Chạy Task xử lý nút bấm (TTP223 Digital) */
    xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);
}

/* ============================================================
 * MQTT HANDLERS
 * ============================================================ */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected to Broker");
            mqtt_connected = true;
            esp_mqtt_client_subscribe(s_mqtt_client, topic_ctrl, 1);
            mqtt_publish_status();
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT Disconnected");
            mqtt_connected = false;
            break;
        case MQTT_EVENT_DATA: {
            char cmd[128] = {0};
            int len = event->data_len < 127 ? event->data_len : 127;
            memcpy(cmd, event->data, len);
            ESP_LOGI(TAG, "MQTT Command RX: %s", cmd);

            if (strstr(cmd, "ON") || strstr(cmd, "\"ON\"")) {
                relay_set(true);
                mqtt_publish_status();
            } else if (strstr(cmd, "OFF") || strstr(cmd, "\"OFF\"")) {
                relay_set(false);
                mqtt_publish_status();
            }
            break;
        }
        default: break;
    }
}

static void start_mqtt(void)
{
    if (s_mqtt_client != NULL) return;

    snprintf(topic_status, sizeof(topic_status), "mylamp_app/device/%s/status", g_device_id);
    snprintf(topic_ctrl, sizeof(topic_ctrl), "mylamp_app/device/%s/control", g_device_id);

    char lwt_msg[128];
    snprintf(lwt_msg, sizeof(lwt_msg), "{\"device_id\":\"%s\",\"online\":false}", g_device_id);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.client_id = g_device_id,
        .session.last_will = {
            .topic = topic_status,
            .msg = lwt_msg,
            .qos = 1,
            .retain = 1,
        },
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);
}

/* ============================================================
 * WI-FI PROVISIONING & CONNECTION HANDLERS
 * ============================================================ */
#define PROV_QR_VERSION "v1"
#define PROV_TRANSPORT_SOFTAP "softap"
#define PROV_TRANSPORT_BLE "ble"
#define QRCODE_BASE_URL "https://espressif.github.io/esp-jumpstart/qrcode.html"

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    static int retries;
    
    /* Log BLE and Provisioning Events */
    if (event_base == PROTOCOMM_TRANSPORT_BLE_EVENT) {
        ESP_LOGI(TAG, "PROTOCOMM_TRANSPORT_BLE_EVENT: %ld", event_id);
        return;
    }
    if (event_base == PROTOCOMM_SECURITY_SESSION_EVENT) {
        ESP_LOGI(TAG, "PROTOCOMM_SECURITY_SESSION_EVENT: %ld", event_id);
        return;
    }
    
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "Provisioning started");
                break;
            case WIFI_PROV_CRED_RECV: {
                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                ESP_LOGI(TAG, "Received Wi-Fi credentials\n\tSSID : %s\n\tPassword : %s",
                         (const char *)wifi_sta_cfg->ssid,
                         (const char *)wifi_sta_cfg->password);
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
                ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s",
                         (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Wi-Fi authentication failed" : "Wi-Fi AP not found");
                retries++;
                if (retries >= CONFIG_EXAMPLE_PROV_MGR_MAX_RETRY_CNT) {
                    ESP_LOGI(TAG, "Failed to connect with provisioned AP, resetting credentials");
                    wifi_prov_mgr_reset_sm_state_on_failure();
                    retries = 0;
                }
                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "Provisioning successful");
                retries = 0;
                break;
            case WIFI_PROV_END:
                ESP_LOGI(TAG, "Provisioning ended");
                wifi_prov_mgr_deinit();
                break;
            default: break;
        }
    } else if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
                ESP_LOGI(TAG, "Disconnected. Reason: %d. Connecting to the AP again...", disconn->reason);
                esp_wifi_connect();
                break;
            default: break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
    }
}

static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = "ESP32_IoT_";
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X", ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
    
    /* Đồng thời lưu làm MQTT Device ID dựa trên MAC address */
    snprintf(g_device_id, sizeof(g_device_id), "esp32_relay_%02X%02X%02X", eth_mac[3], eth_mac[4], eth_mac[5]);
}

esp_err_t custom_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                   uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    if (inbuf) {
        ESP_LOGI(TAG, "Received custom user data: %.*s", inlen, (char *)inbuf);
    }
    char response[] = "SUCCESS";
    *outbuf = (uint8_t *)strdup(response);
    if (*outbuf == NULL) {
        ESP_LOGE(TAG, "System out of memory");
        return ESP_ERR_NO_MEM;
    }
    *outlen = strlen(response) + 1;
    return ESP_OK;
}

static void wifi_prov_print_qr(const char *name, const char *username, const char *pop, const char *transport)
{
    if (!name || !transport) {
        ESP_LOGW(TAG, "Cannot generate QR code payload. Data missing.");
        return;
    }
    
    char payload[150] = {0};
    int payload_len = 0;
    
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char mac_str[20];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if (username && pop) {
        payload_len = snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\",\"mac\":\"%s\",\"username\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}",
                 PROV_QR_VERSION, name, mac_str, username, pop, transport);
    } else if (pop) {
        payload_len = snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\",\"mac\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}",
                 PROV_QR_VERSION, name, mac_str, pop, transport);
    } else {
        payload_len = snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\",\"mac\":\"%s\",\"transport\":\"%s\"}",
                 PROV_QR_VERSION, name, mac_str, transport);
    }
    
    ESP_LOGI(TAG, "QR Code Payload (%d bytes): %s", payload_len, payload);
    ESP_LOGI(TAG, "Scan this QR code from the provisioning application for Provisioning.");
    
    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    /* Generate and display QR code in console */
    esp_qrcode_generate(&cfg, payload);

    ESP_LOGI(TAG, "If QR code is not visible, copy paste the below URL in a browser:\n%s?data=%s", QRCODE_BASE_URL, payload);
    ESP_LOGI(TAG, "Device Name for BLE Discovery: %s", name);
}

/* ============================================================
 * MAIN APPLICATION
 * ============================================================ */
void app_main(void)
{
    /* 1. Init NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* 2. Init Network & Event Loop */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_event_group = xEventGroupCreate();

    /* 3. Đăng ký Handler Event */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    /* 4a. Initialize Bluetooth Controller for BLE */
    ESP_LOGI(TAG, "Initializing Bluetooth Controller...");
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t bt_err = esp_bt_controller_init(&bt_cfg);
    if (bt_err != ESP_OK) {
        ESP_LOGW(TAG, "BT Controller init failed: %s", esp_err_to_name(bt_err));
    } else {
        esp_bt_controller_enable(ESP_BT_MODE_BLE);
        ESP_LOGI(TAG, "Bluetooth Controller initialized (BLE mode)");
    }

    /* 4. Khởi tạo Hardware Thực Thi (Relay & Nút bấm) */
    hw_init();

    /* 5. Set UUID BEFORE initializing provisioning manager */
    ESP_LOGI(TAG, "Setting BLE Service UUID");
    wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);
    ESP_LOGI(TAG, "BLE Service UUID set");

    /* 6. Initialize provisioning manager */
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BT
    };
    ESP_LOGI(TAG, "Initializing WiFi provisioning manager with BLE scheme...");
    esp_err_t prov_init_err = wifi_prov_mgr_init(config);
    if (prov_init_err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi provisioning manager init failed: %s", esp_err_to_name(prov_init_err));
        ESP_ERROR_CHECK(prov_init_err);
    }
    ESP_LOGI(TAG, "WiFi provisioning manager initialized successfully");

    bool provisioned = false;
    
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned) {
        ESP_LOGI(TAG, "Starting Wi-Fi provisioning via BLE");

        char service_name[24];
        get_device_service_name(service_name, sizeof(service_name));
        ESP_LOGI(TAG, "BLE Service Name: %s", service_name);

        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
        const char *username = NULL;
        const char *pop = EXAMPLE_PROV_SEC2_PWD;

        const void *sec_params = (const void *)pop;
        const char *service_key = NULL;

        wifi_prov_mgr_endpoint_create("custom-data");
        ESP_LOGI(TAG, "Created custom endpoint");

        ESP_LOGI(TAG, "Starting provisioning with security SECURITY_1...");
        esp_err_t prov_start_err = wifi_prov_mgr_start_provisioning(security, sec_params, service_name, service_key);
        if (prov_start_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start provisioning: 0x%x (%s)", prov_start_err, esp_err_to_name(prov_start_err));
            ESP_LOGE(TAG, "Provisioning cannot continue. Check BLE and provisioning configuration.");
            while (1) {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        } else {
            ESP_LOGI(TAG, "Provisioning started successfully");
        }
        
        wifi_prov_mgr_endpoint_register("custom-data", custom_prov_data_handler, NULL);
        ESP_LOGI(TAG, "Registered custom endpoint");

        /* Add small delay to ensure BLE is started */
        ESP_LOGI(TAG, "Waiting for BLE to stabilize...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        /* In mã QR ra màn hình Console cho App Quét */
        ESP_LOGI(TAG, "=== DISPLAYING QR CODE ===");
        wifi_prov_print_qr(service_name, username, pop, PROV_TRANSPORT_BLE);
        ESP_LOGI(TAG, "=== QR CODE DISPLAYED ===");
        ESP_LOGI(TAG, "Waiting for provisioning to complete...");
        
        /* Wait for provisioning to complete */
        wifi_prov_mgr_wait();
        wifi_prov_mgr_deinit();

    } else {
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");
        wifi_prov_mgr_deinit();
        
        char service_name[24];
        get_device_service_name(service_name, sizeof(service_name)); /* Lấy Device ID để dùng cho MQTT */

        wifi_init_sta();
    }

    /* Đợi tới khi Wi-Fi được kết nối (nhận IP tĩnh/động từ Router) */
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, pdFALSE, pdTRUE, portMAX_DELAY);

    /* 6. Khi đã có Wi-Fi, bắt đầu MQTT Client */
    ESP_LOGI(TAG, "Wi-Fi is connected. Starting MQTT Client...");
    start_mqtt();

    /* 7. Heartbeat Task (Báo cáo trạng thái theo chu kỳ mỗi 30s) */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        mqtt_publish_status();
    }
}
