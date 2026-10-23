#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    bool connected;
    bool configured;
    int8_t rssi;
    uint32_t reconnect_count;
    char ssid[33];
    char ip[16];
    char last_error[48];
} wifi_manager_snapshot_t;

esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_apply_settings(void);
esp_err_t wifi_manager_reconnect(void);
bool wifi_manager_is_connected(void);
bool wifi_manager_is_time_valid(void);
void wifi_manager_get_snapshot(wifi_manager_snapshot_t *snapshot);
