#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define SETTINGS_WIFI_SSID_MAX 33U
#define SETTINGS_WIFI_PASSWORD_MAX 65U
#define SETTINGS_EDGE_URL_MAX 192U
#define SETTINGS_EDGE_KEY_MAX 128U
#define SETTINGS_LLM_MODEL_MAX 65U
#define SETTINGS_DEVICE_ID_MAX 65U
#define SETTINGS_ADMIN_PIN_MAX 7U
#define SETTINGS_FORECAST_URL_MAX 192U
#define SETTINGS_FORECAST_KEY_MAX 128U
#define SETTINGS_LOCATION_NAME_MAX 65U
#define SETTINGS_TIMEZONE_MAX 33U

typedef struct {
    char wifi_ssid[SETTINGS_WIFI_SSID_MAX];
    char wifi_password[SETTINGS_WIFI_PASSWORD_MAX];
    char edge_api_url[SETTINGS_EDGE_URL_MAX];
    char edge_key[SETTINGS_EDGE_KEY_MAX];
    char llm_model[SETTINGS_LLM_MODEL_MAX];
    char device_id[SETTINGS_DEVICE_ID_MAX];
    char admin_pin[SETTINGS_ADMIN_PIN_MAX];
    uint32_t ai_auto_interval_seconds;
    char forecast_api_url[SETTINGS_FORECAST_URL_MAX];
    char forecast_key[SETTINGS_FORECAST_KEY_MAX];
    char location_name[SETTINGS_LOCATION_NAME_MAX];
    char timezone[SETTINGS_TIMEZONE_MAX];
    float latitude;
    float longitude;
    uint32_t forecast_refresh_seconds;
    bool using_saved_values;
} settings_snapshot_t;

esp_err_t settings_manager_init(void);
void settings_manager_get_snapshot(settings_snapshot_t *snapshot);
esp_err_t settings_manager_save(const settings_snapshot_t *snapshot, char *error, size_t error_size);
esp_err_t settings_manager_save_network(const settings_snapshot_t *snapshot, char *error, size_t error_size);
esp_err_t settings_manager_save_edge(const settings_snapshot_t *snapshot, char *error, size_t error_size);
esp_err_t settings_manager_save_forecast(const settings_snapshot_t *snapshot, char *error, size_t error_size);
esp_err_t settings_manager_save_pin(const char *pin, char *error, size_t error_size);
esp_err_t settings_manager_restore_defaults(void);
bool settings_manager_verify_pin(const char *pin);
void settings_manager_mask_secret(const char *secret, char *masked, size_t masked_size);
