#include "wifi_manager.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "settings_manager.h"

static const char *TAG = "wifi_manager";
static const EventBits_t WIFI_CONNECTED_BIT = BIT0;
static EventGroupHandle_t s_events;
static SemaphoreHandle_t s_lock;
static esp_netif_t *s_station_netif;
static wifi_manager_snapshot_t s_snapshot;
static bool s_sntp_started;

static void wifi_manager_start_sntp(void)
{
    if (s_sntp_started) {
        return;
    }
    const esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");
    if (esp_netif_sntp_init(&config) == ESP_OK) {
        s_sntp_started = true;
        ESP_LOGI(TAG, "SNTP started for HTTPS certificate validation");
    } else {
        ESP_LOGW(TAG, "cannot start SNTP");
    }
}

static const char *wifi_disconnect_reason_name(uint8_t reason)
{
    switch (reason) {
    case WIFI_REASON_NO_AP_FOUND:
        return "AP not found (SSID, 2.4 GHz, or signal)";
    case WIFI_REASON_AUTH_FAIL:
        return "authentication failed (check password/security)";
    case WIFI_REASON_ASSOC_FAIL:
        return "association failed";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        return "security handshake timeout";
    case WIFI_REASON_BEACON_TIMEOUT:
        return "beacon timeout (weak signal)";
    default:
        return "station disconnected";
    }
}

static bool wifi_manager_is_configured(void)
{
    bool configured;
    (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    configured = s_snapshot.configured;
    (void)xSemaphoreGive(s_lock);
    return configured;
}

static void wifi_manager_connect_if_configured(void)
{
    if (wifi_manager_is_configured()) {
        esp_err_t result = esp_wifi_connect();
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(result));
        } else {
            ESP_LOGI(TAG, "requesting Wi-Fi connection");
        }
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "station started");
        wifi_manager_connect_if_configured();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        const wifi_event_sta_connected_t *connected = data;
        ESP_LOGI(TAG, "associated with SSID '%.*s' on channel %u",
                 connected != NULL ? connected->ssid_len : 0,
                 connected != NULL ? (const char *)connected->ssid : "",
                 connected != NULL ? connected->channel : 0);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *disconnected = data;
        uint8_t reason = disconnected != NULL ? disconnected->reason : 0U;
        char error[sizeof(s_snapshot.last_error)];
        (void)snprintf(error, sizeof(error), "Wi-Fi reason %u", reason);
        xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT);
        (void)xSemaphoreTake(s_lock, portMAX_DELAY);
        s_snapshot.connected = false;
        s_snapshot.ip[0] = '\0';
        s_snapshot.reconnect_count++;
        strlcpy(s_snapshot.last_error, error, sizeof(s_snapshot.last_error));
        (void)xSemaphoreGive(s_lock);
        ESP_LOGW(TAG, "Wi-Fi disconnected: reason=%u, %s; reconnect #%lu",
                 reason, wifi_disconnect_reason_name(reason),
                 (unsigned long)s_snapshot.reconnect_count);
        wifi_manager_connect_if_configured();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_events, WIFI_CONNECTED_BIT);
        (void)xSemaphoreTake(s_lock, portMAX_DELAY);
        s_snapshot.connected = true;
        s_snapshot.last_error[0] = '\0';
        if (s_station_netif != NULL) {
            esp_netif_ip_info_t info = {0};
            if (esp_netif_get_ip_info(s_station_netif, &info) == ESP_OK) {
                (void)esp_ip4addr_ntoa(&info.ip, s_snapshot.ip, sizeof(s_snapshot.ip));
            }
        }
        (void)xSemaphoreGive(s_lock);
        wifi_manager_start_sntp();
        ESP_LOGI(TAG, "Wi-Fi got IPv4 address %s", s_snapshot.ip);
    }
}

esp_err_t wifi_manager_apply_settings(void)
{
    settings_snapshot_t settings;
    wifi_config_t config = {0};

    settings_manager_get_snapshot(&settings);
    strlcpy((char *)config.sta.ssid, settings.wifi_ssid, sizeof(config.sta.ssid));
    strlcpy((char *)config.sta.password, settings.wifi_password, sizeof(config.sta.password));
    /* Let the driver negotiate with open, WPA2 and WPA3 access points. */
    config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &config), TAG, "set station config");
    ESP_LOGI(TAG, "apply Wi-Fi: SSID='%s' (%u chars), password=%s",
             settings.wifi_ssid, (unsigned)strlen(settings.wifi_ssid),
             settings.wifi_password[0] != '\0' ? "configured" : "empty");

    (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    s_snapshot.configured = settings.wifi_ssid[0] != '\0';
    strlcpy(s_snapshot.ssid, settings.wifi_ssid, sizeof(s_snapshot.ssid));
    s_snapshot.connected = false;
    s_snapshot.ip[0] = '\0';
    strlcpy(s_snapshot.last_error, s_snapshot.configured ? "Connecting" : "SSID not configured", sizeof(s_snapshot.last_error));
    (void)xSemaphoreGive(s_lock);
    xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT);
    (void)esp_wifi_disconnect();
    wifi_manager_connect_if_configured();
    return ESP_OK;
}

esp_err_t wifi_manager_reconnect(void)
{
    return wifi_manager_apply_settings();
}

esp_err_t wifi_manager_init(void)
{
    s_events = xEventGroupCreate();
    s_lock = xSemaphoreCreateMutex();
    if (s_events == NULL || s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "initialize network interface");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "create event loop");
    s_station_netif = esp_netif_create_default_wifi_sta();
    if (s_station_netif == NULL) {
        return ESP_FAIL;
    }
    const wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_config), TAG, "initialize Wi-Fi");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                            wifi_event_handler, NULL, NULL), TAG, "register Wi-Fi event");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                            wifi_event_handler, NULL, NULL), TAG, "register IP event");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set station mode");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start Wi-Fi");
    ESP_LOGI(TAG, "Wi-Fi station initialized; serial diagnostics are on UART0 at 115200 baud");
    return wifi_manager_apply_settings();
}

bool wifi_manager_is_connected(void)
{
    return s_events != NULL && (xEventGroupGetBits(s_events) & WIFI_CONNECTED_BIT) != 0U;
}

bool wifi_manager_is_time_valid(void)
{
    time_t now = 0;
    time(&now);
    // TLS certificate validation needs a date later than the first current public CA validity period.
    return now >= 1700000000;
}

void wifi_manager_get_snapshot(wifi_manager_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }
    *snapshot = (wifi_manager_snapshot_t){0};
    if (s_lock == NULL) {
        return;
    }
    (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    *snapshot = s_snapshot;
    (void)xSemaphoreGive(s_lock);
    if (snapshot->connected) {
        wifi_ap_record_t ap = {0};
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            snapshot->rssi = ap.rssi;
        }
    }
}
