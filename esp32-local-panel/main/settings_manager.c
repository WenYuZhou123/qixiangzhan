#include "settings_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "settings";
static const char *NVS_NAMESPACE = "panel_cfg";
/* Revision 2 migrates installations that saved the retired direct-model or
 * pre-production Edge endpoints. Site-owned Wi-Fi and location fields stay
 * untouched by the profile migration. */
#define SETTINGS_SERVICE_PROFILE_REVISION 2U
static settings_snapshot_t s_settings;
static SemaphoreHandle_t s_lock;

static void settings_copy(char *destination, size_t size, const char *source)
{
    if (size > 0U) {
        strlcpy(destination, source != NULL ? source : "", size);
    }
}

static void settings_error(char *error, size_t size, const char *message)
{
    if (error != NULL && size > 0U) {
        settings_copy(error, size, message);
    }
}

static void settings_defaults(settings_snapshot_t *settings)
{
    *settings = (settings_snapshot_t){0};
    settings_copy(settings->wifi_ssid, sizeof(settings->wifi_ssid), CONFIG_PANEL_WIFI_SSID);
    settings_copy(settings->wifi_password, sizeof(settings->wifi_password), CONFIG_PANEL_WIFI_PASSWORD);
    settings_copy(settings->edge_api_url, sizeof(settings->edge_api_url), CONFIG_PANEL_EDGE_API_URL);
    settings_copy(settings->edge_key, sizeof(settings->edge_key), CONFIG_PANEL_EDGE_KEY);
    settings_copy(settings->llm_model, sizeof(settings->llm_model), CONFIG_PANEL_LLM_MODEL);
    settings_copy(settings->device_id, sizeof(settings->device_id), CONFIG_PANEL_EDGE_DEVICE_ID);
    settings_copy(settings->admin_pin, sizeof(settings->admin_pin), CONFIG_PANEL_ADMIN_PIN);
    settings->ai_auto_interval_seconds = CONFIG_PANEL_AI_AUTO_INTERVAL_SECONDS;
    settings_copy(settings->forecast_api_url, sizeof(settings->forecast_api_url), CONFIG_PANEL_FORECAST_API_URL);
    settings_copy(settings->forecast_key, sizeof(settings->forecast_key), CONFIG_PANEL_FORECAST_KEY);
    settings_copy(settings->location_name, sizeof(settings->location_name), CONFIG_PANEL_FORECAST_LOCATION_NAME);
    settings_copy(settings->timezone, sizeof(settings->timezone), CONFIG_PANEL_FORECAST_TIMEZONE);
    settings->latitude = strtof(CONFIG_PANEL_FORECAST_LATITUDE, NULL);
    settings->longitude = strtof(CONFIG_PANEL_FORECAST_LONGITUDE, NULL);
    settings->forecast_refresh_seconds = CONFIG_PANEL_FORECAST_REFRESH_SECONDS;
    settings->using_saved_values = false;
}

/* Service credentials are provisioned by the firmware. Location and Wi-Fi are
 * intentionally excluded because they belong to the installation site. */
static void settings_apply_service_profile(settings_snapshot_t *settings,
                                           const settings_snapshot_t *defaults)
{
    settings_copy(settings->edge_api_url, sizeof(settings->edge_api_url), defaults->edge_api_url);
    settings_copy(settings->edge_key, sizeof(settings->edge_key), defaults->edge_key);
    settings_copy(settings->llm_model, sizeof(settings->llm_model), defaults->llm_model);
    settings_copy(settings->device_id, sizeof(settings->device_id), defaults->device_id);
    settings->ai_auto_interval_seconds = defaults->ai_auto_interval_seconds;
    settings_copy(settings->forecast_api_url, sizeof(settings->forecast_api_url), defaults->forecast_api_url);
    settings_copy(settings->forecast_key, sizeof(settings->forecast_key), defaults->forecast_key);
    settings_copy(settings->timezone, sizeof(settings->timezone), defaults->timezone);
    settings->forecast_refresh_seconds = defaults->forecast_refresh_seconds;
}

static esp_err_t settings_write_service_profile(nvs_handle_t handle, const settings_snapshot_t *settings)
{
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "edge_url", settings->edge_api_url), TAG, "save profile AI URL");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "edge_key", settings->edge_key), TAG, "save profile AI key");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "llm_model", settings->llm_model), TAG, "save profile model");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "device_id", settings->device_id), TAG, "save profile ID");
    ESP_RETURN_ON_ERROR(nvs_set_u32(handle, "ai_period", settings->ai_auto_interval_seconds), TAG, "save profile AI period");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "fc_url", settings->forecast_api_url), TAG, "save profile forecast URL");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "fc_key", settings->forecast_key), TAG, "save profile forecast key");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "fc_timezone", settings->timezone), TAG, "save profile timezone");
    ESP_RETURN_ON_ERROR(nvs_set_u32(handle, "fc_period", settings->forecast_refresh_seconds), TAG, "save profile forecast period");
    ESP_RETURN_ON_ERROR(nvs_set_u32(handle, "svc_rev", SETTINGS_SERVICE_PROFILE_REVISION), TAG, "save profile revision");
    return nvs_commit(handle);
}

static bool settings_read_string(nvs_handle_t handle, const char *key, char *value, size_t value_size)
{
    size_t required_size = value_size;
    return nvs_get_str(handle, key, value, &required_size) == ESP_OK;
}

static bool settings_is_direct_model(const settings_snapshot_t *settings)
{
    static const char *suffix = "/chat/completions";
    size_t url_length = strlen(settings->edge_api_url);
    return url_length >= strlen(suffix) && strcmp(&settings->edge_api_url[url_length - strlen(suffix)], suffix) == 0;
}

static bool settings_is_legacy_service_url(const char *url)
{
    return url != NULL &&
           strcmp(url, "https://ws-hnatakb75l05x99p.cn-beijing.maas.aliyuncs.com/compatible-mode/v1/chat/completions") == 0;
}

static bool settings_validate_service(const settings_snapshot_t *settings, char *error, size_t error_size)
{
    static const char *edge_suffix = "/api/v1/edge/ai/analyze";
    size_t url_length;

    if (strncmp(settings->edge_api_url, "https://", 8U) != 0) {
        settings_error(error, error_size, "AI API must use HTTPS");
        return false;
    }
    url_length = strlen(settings->edge_api_url);
    if (settings->edge_key[0] == '\0') {
        settings_error(error, error_size, "API key required");
        return false;
    }
    if (settings_is_direct_model(settings)) {
        if (settings->llm_model[0] == '\0') {
            settings_error(error, error_size, "Model name required");
            return false;
        }
    } else if (url_length < strlen(edge_suffix) ||
               strcmp(&settings->edge_api_url[url_length - strlen(edge_suffix)], edge_suffix) != 0) {
        settings_error(error, error_size, "Use /chat/completions or the Edge endpoint");
        return false;
    } else if (settings->device_id[0] == '\0') {
        settings_error(error, error_size, "Device ID required");
        return false;
    }
    if (settings->ai_auto_interval_seconds < 30U || settings->ai_auto_interval_seconds > 3600U) {
        settings_error(error, error_size, "AI interval must be 30-3600 seconds");
        return false;
    }
    settings_error(error, error_size, "");
    return true;
}

static bool settings_validate(const settings_snapshot_t *settings, char *error, size_t error_size)
{
    if (settings->wifi_ssid[0] == '\0') {
        settings_error(error, error_size, "Wi-Fi SSID required");
        return false;
    }
    if (!settings_validate_service(settings, error, error_size)) {
        return false;
    }
    if (strlen(settings->admin_pin) < 4U || strlen(settings->admin_pin) >= SETTINGS_ADMIN_PIN_MAX) {
        settings_error(error, error_size, "Administrator PIN must contain 4-6 digits");
        return false;
    }
    for (const char *cursor = settings->admin_pin; *cursor != '\0'; cursor++) {
        if (*cursor < '0' || *cursor > '9') {
            settings_error(error, error_size, "Administrator PIN must contain digits only");
            return false;
        }
    }
    settings_error(error, error_size, "");
    return true;
}

static bool settings_validate_network(const settings_snapshot_t *settings, char *error, size_t error_size)
{
    if (settings->wifi_ssid[0] == '\0') {
        settings_error(error, error_size, "Wi-Fi SSID required");
        return false;
    }
    settings_error(error, error_size, "");
    return true;
}

static bool settings_validate_forecast(const settings_snapshot_t *settings, char *error, size_t error_size)
{
    static const char *suffix = "/api/v1/edge/weather/forecast";
    size_t length = strlen(settings->forecast_api_url);
    if (strncmp(settings->forecast_api_url, "https://", 8U) != 0 ||
        length < strlen(suffix) || strcmp(&settings->forecast_api_url[length - strlen(suffix)], suffix) != 0) {
        settings_error(error, error_size, "Forecast URL must use the edge endpoint");
        return false;
    }
    if (settings->forecast_key[0] == '\0' || settings->location_name[0] == '\0' || settings->timezone[0] == '\0') {
        settings_error(error, error_size, "Forecast key, location and timezone are required");
        return false;
    }
    if (settings->latitude < -90.0f || settings->latitude > 90.0f ||
        settings->longitude < -180.0f || settings->longitude > 180.0f ||
        settings->forecast_refresh_seconds < 300U || settings->forecast_refresh_seconds > 7200U) {
        settings_error(error, error_size, "Forecast coordinates or refresh interval are invalid");
        return false;
    }
    settings_error(error, error_size, "");
    return true;
}

static bool settings_validate_edge(const settings_snapshot_t *settings, char *error, size_t error_size)
{
    return settings_validate_service(settings, error, error_size);
}

static esp_err_t settings_write(nvs_handle_t handle, const settings_snapshot_t *settings)
{
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "wifi_ssid", settings->wifi_ssid), TAG, "save SSID");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "wifi_pass", settings->wifi_password), TAG, "save Wi-Fi password");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "edge_url", settings->edge_api_url), TAG, "save API URL");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "edge_key", settings->edge_key), TAG, "save edge key");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "llm_model", settings->llm_model), TAG, "save model");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "device_id", settings->device_id), TAG, "save device ID");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "admin_pin", settings->admin_pin), TAG, "save administrator PIN");
    ESP_RETURN_ON_ERROR(nvs_set_u32(handle, "ai_period", settings->ai_auto_interval_seconds), TAG, "save AI interval");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "fc_url", settings->forecast_api_url), TAG, "save forecast URL");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "fc_key", settings->forecast_key), TAG, "save forecast key");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "fc_location", settings->location_name), TAG, "save forecast location");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "fc_timezone", settings->timezone), TAG, "save forecast timezone");
    ESP_RETURN_ON_ERROR(nvs_set_blob(handle, "fc_coords", &settings->latitude, sizeof(float) * 2U), TAG, "save forecast coordinates");
    ESP_RETURN_ON_ERROR(nvs_set_u32(handle, "fc_period", settings->forecast_refresh_seconds), TAG, "save forecast interval");
    return nvs_commit(handle);
}

esp_err_t settings_manager_init(void)
{
    nvs_handle_t handle;
    esp_err_t error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "erase NVS");
        error = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(error, TAG, "initialize NVS");
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }
    settings_defaults(&s_settings);
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return ESP_OK;
    }
    settings_snapshot_t defaults = s_settings;
    settings_snapshot_t loaded = s_settings;
    uint32_t interval = loaded.ai_auto_interval_seconds;
    uint32_t forecast_interval = loaded.forecast_refresh_seconds;
    uint32_t service_profile_revision = 0U;
    size_t forecast_coordinates_size = sizeof(float) * 2U;
    bool loaded_any = false;
    loaded_any |= settings_read_string(handle, "wifi_ssid", loaded.wifi_ssid, sizeof(loaded.wifi_ssid));
    loaded_any |= settings_read_string(handle, "wifi_pass", loaded.wifi_password, sizeof(loaded.wifi_password));
    loaded_any |= settings_read_string(handle, "edge_url", loaded.edge_api_url, sizeof(loaded.edge_api_url));
    loaded_any |= settings_read_string(handle, "edge_key", loaded.edge_key, sizeof(loaded.edge_key));
    loaded_any |= settings_read_string(handle, "llm_model", loaded.llm_model, sizeof(loaded.llm_model));
    loaded_any |= settings_read_string(handle, "device_id", loaded.device_id, sizeof(loaded.device_id));
    loaded_any |= settings_read_string(handle, "admin_pin", loaded.admin_pin, sizeof(loaded.admin_pin));
    loaded_any |= settings_read_string(handle, "fc_url", loaded.forecast_api_url, sizeof(loaded.forecast_api_url));
    loaded_any |= settings_read_string(handle, "fc_key", loaded.forecast_key, sizeof(loaded.forecast_key));
    loaded_any |= settings_read_string(handle, "fc_location", loaded.location_name, sizeof(loaded.location_name));
    loaded_any |= settings_read_string(handle, "fc_timezone", loaded.timezone, sizeof(loaded.timezone));
    if (nvs_get_blob(handle, "fc_coords", &loaded.latitude, &forecast_coordinates_size) == ESP_OK &&
        forecast_coordinates_size == sizeof(float) * 2U) {
        loaded_any = true;
    }
    if (nvs_get_u32(handle, "ai_period", &interval) == ESP_OK) {
        loaded.ai_auto_interval_seconds = interval;
        loaded_any = true;
    }
    if (nvs_get_u32(handle, "fc_period", &forecast_interval) == ESP_OK) {
        loaded.forecast_refresh_seconds = forecast_interval;
        loaded_any = true;
    }
    (void)nvs_get_u32(handle, "svc_rev", &service_profile_revision);
    nvs_close(handle);
    if (loaded_any) {
        /* Older installations may have persisted empty service fields. Keep the
         * newly provisioned direct endpoint without discarding saved Wi-Fi. */
        if (loaded.edge_api_url[0] == '\0' && defaults.edge_api_url[0] != '\0') {
            settings_copy(loaded.edge_api_url, sizeof(loaded.edge_api_url), defaults.edge_api_url);
        }
        if (loaded.edge_key[0] == '\0' && defaults.edge_key[0] != '\0') {
            settings_copy(loaded.edge_key, sizeof(loaded.edge_key), defaults.edge_key);
        }
        if (loaded.llm_model[0] == '\0' && defaults.llm_model[0] != '\0') {
            settings_copy(loaded.llm_model, sizeof(loaded.llm_model), defaults.llm_model);
        }
        if (loaded.forecast_api_url[0] == '\0' && defaults.forecast_api_url[0] != '\0') {
            settings_copy(loaded.forecast_api_url, sizeof(loaded.forecast_api_url), defaults.forecast_api_url);
        }
        if (strcmp(loaded.llm_model, "qwen-plus") == 0 &&
            strcmp(defaults.llm_model, "qwen-plus") != 0) {
            settings_copy(loaded.llm_model, sizeof(loaded.llm_model), defaults.llm_model);
        }
        if (settings_is_legacy_service_url(loaded.edge_api_url)) {
            settings_copy(loaded.edge_api_url, sizeof(loaded.edge_api_url), defaults.edge_api_url);
            settings_copy(loaded.edge_key, sizeof(loaded.edge_key), defaults.edge_key);
        }
        if (strcmp(loaded.forecast_api_url,
                   "https://api.qixiangzhan.online/api/v1/edge/weather/forecast") == 0) {
            settings_copy(loaded.forecast_api_url, sizeof(loaded.forecast_api_url), defaults.forecast_api_url);
            settings_copy(loaded.forecast_key, sizeof(loaded.forecast_key), defaults.forecast_key);
        }
        loaded.using_saved_values = true;
    }
    if (service_profile_revision < SETTINGS_SERVICE_PROFILE_REVISION) {
        settings_apply_service_profile(&loaded, &defaults);
        ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle), TAG, "open profile NVS");
        error = settings_write_service_profile(handle, &loaded);
        nvs_close(handle);
        ESP_RETURN_ON_ERROR(error, TAG, "provision service profile");
        ESP_LOGI(TAG, "provisioned service profile revision %u", SETTINGS_SERVICE_PROFILE_REVISION);
    }
    s_settings = loaded;
    return ESP_OK;
}

void settings_manager_get_snapshot(settings_snapshot_t *snapshot)
{
    if (snapshot == NULL || s_lock == NULL) {
        return;
    }
    (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    *snapshot = s_settings;
    (void)xSemaphoreGive(s_lock);
}

esp_err_t settings_manager_save(const settings_snapshot_t *snapshot, char *error, size_t error_size)
{
    nvs_handle_t handle;
    esp_err_t result;
    settings_snapshot_t next;

    if (snapshot == NULL || !settings_validate(snapshot, error, error_size)) {
        return ESP_ERR_INVALID_ARG;
    }
    next = *snapshot;
    next.using_saved_values = true;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle), TAG, "open NVS");
    result = settings_write(handle, &next);
    nvs_close(handle);
    if (result != ESP_OK) {
        settings_error(error, error_size, "Cannot save NVS settings");
        return result;
    }
    (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    s_settings = next;
    (void)xSemaphoreGive(s_lock);
    settings_error(error, error_size, "");
    return ESP_OK;
}

esp_err_t settings_manager_save_network(const settings_snapshot_t *snapshot, char *error, size_t error_size)
{
    nvs_handle_t handle;
    settings_snapshot_t next;
    if (snapshot == NULL || !settings_validate_network(snapshot, error, error_size)) return ESP_ERR_INVALID_ARG;
    settings_manager_get_snapshot(&next);
    settings_copy(next.wifi_ssid, sizeof(next.wifi_ssid), snapshot->wifi_ssid);
    settings_copy(next.wifi_password, sizeof(next.wifi_password), snapshot->wifi_password);
    next.using_saved_values = true;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle), TAG, "open NVS");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "wifi_ssid", next.wifi_ssid), TAG, "save SSID");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "wifi_pass", next.wifi_password), TAG, "save password");
    esp_err_t result = nvs_commit(handle);
    nvs_close(handle);
    if (result == ESP_OK) { (void)xSemaphoreTake(s_lock, portMAX_DELAY); s_settings = next; (void)xSemaphoreGive(s_lock); }
    return result;
}

esp_err_t settings_manager_save_edge(const settings_snapshot_t *snapshot, char *error, size_t error_size)
{
    nvs_handle_t handle;
    settings_snapshot_t next;
    if (snapshot == NULL || !settings_validate_edge(snapshot, error, error_size)) return ESP_ERR_INVALID_ARG;
    settings_manager_get_snapshot(&next);
    settings_copy(next.edge_api_url, sizeof(next.edge_api_url), snapshot->edge_api_url);
    settings_copy(next.edge_key, sizeof(next.edge_key), snapshot->edge_key);
    settings_copy(next.llm_model, sizeof(next.llm_model), snapshot->llm_model);
    settings_copy(next.device_id, sizeof(next.device_id), snapshot->device_id);
    next.ai_auto_interval_seconds = snapshot->ai_auto_interval_seconds;
    next.using_saved_values = true;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle), TAG, "open NVS");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "edge_url", next.edge_api_url), TAG, "save URL");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "edge_key", next.edge_key), TAG, "save key");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "llm_model", next.llm_model), TAG, "save model");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, "device_id", next.device_id), TAG, "save ID");
    ESP_RETURN_ON_ERROR(nvs_set_u32(handle, "ai_period", next.ai_auto_interval_seconds), TAG, "save period");
    esp_err_t result = nvs_commit(handle);
    nvs_close(handle);
    if (result == ESP_OK) { (void)xSemaphoreTake(s_lock, portMAX_DELAY); s_settings = next; (void)xSemaphoreGive(s_lock); }
    return result;
}

esp_err_t settings_manager_save_forecast(const settings_snapshot_t *snapshot, char *error, size_t error_size)
{
    nvs_handle_t handle;
    settings_snapshot_t next;
    esp_err_t result;
    if (snapshot == NULL || !settings_validate_forecast(snapshot, error, error_size)) return ESP_ERR_INVALID_ARG;
    settings_manager_get_snapshot(&next);
    strlcpy(next.forecast_api_url, snapshot->forecast_api_url, sizeof(next.forecast_api_url));
    strlcpy(next.forecast_key, snapshot->forecast_key, sizeof(next.forecast_key));
    strlcpy(next.location_name, snapshot->location_name, sizeof(next.location_name));
    strlcpy(next.timezone, snapshot->timezone, sizeof(next.timezone));
    next.latitude = snapshot->latitude;
    next.longitude = snapshot->longitude;
    next.forecast_refresh_seconds = snapshot->forecast_refresh_seconds;
    next.using_saved_values = true;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle), TAG, "open NVS");
    result = nvs_set_str(handle, "fc_url", next.forecast_api_url);
    if (result == ESP_OK) result = nvs_set_str(handle, "fc_key", next.forecast_key);
    if (result == ESP_OK) result = nvs_set_str(handle, "fc_location", next.location_name);
    if (result == ESP_OK) result = nvs_set_str(handle, "fc_timezone", next.timezone);
    if (result == ESP_OK) result = nvs_set_blob(handle, "fc_coords", &next.latitude, sizeof(float) * 2U);
    if (result == ESP_OK) result = nvs_set_u32(handle, "fc_period", next.forecast_refresh_seconds);
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    if (result == ESP_OK) { (void)xSemaphoreTake(s_lock, portMAX_DELAY); s_settings = next; (void)xSemaphoreGive(s_lock); }
    return result;
}

esp_err_t settings_manager_save_pin(const char *pin, char *error, size_t error_size)
{
    nvs_handle_t handle;
    settings_snapshot_t next;
    settings_manager_get_snapshot(&next);
    settings_copy(next.admin_pin, sizeof(next.admin_pin), pin);
    if (strlen(next.admin_pin) < 4U || strlen(next.admin_pin) >= SETTINGS_ADMIN_PIN_MAX) {
        settings_error(error, error_size, "PIN must contain 4-6 digits");
        return ESP_ERR_INVALID_ARG;
    }
    for (const char *cursor = next.admin_pin; *cursor != '\0'; cursor++) {
        if (*cursor < '0' || *cursor > '9') { settings_error(error, error_size, "PIN must contain digits only"); return ESP_ERR_INVALID_ARG; }
    }
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle), TAG, "open NVS");
    esp_err_t result = nvs_set_str(handle, "admin_pin", next.admin_pin);
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    if (result == ESP_OK) { (void)xSemaphoreTake(s_lock, portMAX_DELAY); s_settings = next; (void)xSemaphoreGive(s_lock); }
    return result;
}

esp_err_t settings_manager_restore_defaults(void)
{
    nvs_handle_t handle;
    settings_snapshot_t defaults;

    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle), TAG, "open NVS");
    ESP_RETURN_ON_ERROR(nvs_erase_all(handle), TAG, "clear settings");
    ESP_RETURN_ON_ERROR(nvs_commit(handle), TAG, "commit reset");
    nvs_close(handle);
    settings_defaults(&defaults);
    (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    s_settings = defaults;
    (void)xSemaphoreGive(s_lock);
    return ESP_OK;
}

bool settings_manager_verify_pin(const char *pin)
{
    settings_snapshot_t settings;
    settings_manager_get_snapshot(&settings);
    return pin != NULL && strcmp(pin, settings.admin_pin) == 0;
}

void settings_manager_mask_secret(const char *secret, char *masked, size_t masked_size)
{
    size_t length;
    if (masked == NULL || masked_size == 0U) {
        return;
    }
    length = secret != NULL ? strlen(secret) : 0U;
    if (length == 0U) {
        settings_copy(masked, masked_size, "Not configured");
    } else {
        size_t bullets = length < 8U ? length : 8U;
        memset(masked, '*', bullets < masked_size - 1U ? bullets : masked_size - 1U);
        masked[bullets < masked_size - 1U ? bullets : masked_size - 1U] = '\0';
    }
}
