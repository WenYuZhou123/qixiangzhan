#include "forecast_client.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "forecast_state.h"
#include "panel_state.h"
#include "settings_manager.h"
#include "wifi_manager.h"

#define FORECAST_REQUEST_BIT BIT0
#define FORECAST_REQUEST_SIZE 1024U
#define FORECAST_RESPONSE_SIZE 8192U
#define FORECAST_TASK_STACK_SIZE 10240U

static const char *TAG = "forecast_client";
static EventGroupHandle_t s_events;

typedef struct {
    char *body;
    size_t length;
    size_t capacity;
    bool overflow;
} forecast_http_response_t;

static bool forecast_is_configured(const settings_snapshot_t *settings)
{
    return settings->forecast_api_url[0] != '\0' && settings->forecast_key[0] != '\0' &&
           settings->location_name[0] != '\0' && settings->timezone[0] != '\0' &&
           settings->latitude >= -90.0f && settings->latitude <= 90.0f &&
           settings->longitude >= -180.0f && settings->longitude <= 180.0f;
}

static esp_err_t forecast_http_event(esp_http_client_event_t *event)
{
    forecast_http_response_t *response = event->user_data;
    if (event->event_id != HTTP_EVENT_ON_DATA || response == NULL || event->data == NULL) return ESP_OK;
    if (response->length + event->data_len >= response->capacity) {
        response->overflow = true;
        return ESP_FAIL;
    }
    memcpy(response->body + response->length, event->data, event->data_len);
    response->length += event->data_len;
    response->body[response->length] = '\0';
    return ESP_OK;
}

static const char *forecast_json_value(const char *object, const char *key)
{
    char token[48];
    const char *value;
    int written = snprintf(token, sizeof(token), "\"%s\"", key);
    if (written <= 0 || (size_t)written >= sizeof(token)) return NULL;
    value = strstr(object, token);
    if (value == NULL || (value = strchr(value + strlen(token), ':')) == NULL) return NULL;
    do { value++; } while (*value == ' ' || *value == '\r' || *value == '\n' || *value == '\t');
    return value;
}

static bool forecast_json_string(const char *object, const char *key, char *destination, size_t size)
{
    const char *value = forecast_json_value(object, key);
    const char *end;
    if (value == NULL || *value++ != '\"' || (end = strchr(value, '\"')) == NULL || (size_t)(end - value) >= size) return false;
    memcpy(destination, value, (size_t)(end - value));
    destination[end - value] = '\0';
    return true;
}

static bool forecast_json_number(const char *object, const char *key, double *value)
{
    char *end;
    const char *text = forecast_json_value(object, key);
    if (text == NULL) return false;
    *value = strtod(text, &end);
    return end != text;
}

static const char *forecast_next_object(const char *cursor, char *destination, size_t size)
{
    const char *start = strchr(cursor, '{');
    const char *end;
    if (start == NULL || (end = strchr(start, '}')) == NULL || (size_t)(end - start + 1U) >= size) return NULL;
    memcpy(destination, start, (size_t)(end - start + 1U));
    destination[end - start + 1U] = '\0';
    return end + 1;
}

static bool forecast_parse_response(const char *body, const settings_snapshot_t *settings,
                                    forecast_state_snapshot_t *result)
{
    const char *hours = strstr(body, "\"hours\"");
    const char *cursor;
    char risk[12];
    char source[20];
    if (hours == NULL || !forecast_json_string(body, "next_three_hour_risk", risk, sizeof(risk))) return false;
    *result = (forecast_state_snapshot_t){0};
    result->configured = true;
    result->next_three_hour_risk = forecast_risk_from_name(risk);
    result->local_correction_applied = forecast_json_value(body, "local_correction_applied") != NULL &&
                                       strncmp(forecast_json_value(body, "local_correction_applied"), "true", 4) == 0;
    strlcpy(result->source, forecast_json_string(body, "source", source, sizeof(source)) ? source : "open-meteo", sizeof(result->source));
    strlcpy(result->location_name, settings->location_name, sizeof(result->location_name));
    result->expires_at_ms = (uint32_t)(esp_timer_get_time() / 1000LL) + settings->forecast_refresh_seconds * 1000U;
    cursor = strchr(hours, '[');
    if (cursor == NULL) return false;
    for (uint32_t index = 0; index < FORECAST_HOUR_COUNT; index++) {
        char object[512];
        char hour_risk[12];
        double value;
        cursor = forecast_next_object(cursor, object, sizeof(object));
        if (cursor == NULL || !forecast_json_string(object, "time", result->hours[index].time, sizeof(result->hours[index].time)) ||
            !forecast_json_string(object, "risk_level", hour_risk, sizeof(hour_risk)) ||
            !forecast_json_number(object, "temperature_c", &value)) return false;
        result->hours[index].temperature_c = (float)value;
        if (!forecast_json_number(object, "humidity_rh", &value)) return false;
        result->hours[index].humidity_rh = (uint8_t)value;
        if (!forecast_json_number(object, "precipitation_probability", &value)) return false;
        result->hours[index].precipitation_probability = (uint8_t)value;
        if (!forecast_json_number(object, "precipitation_mm", &value)) return false;
        result->hours[index].precipitation_mm = (float)value;
        if (!forecast_json_number(object, "wind_speed_mps", &value)) return false;
        result->hours[index].wind_speed_mps = (float)value;
        if (!forecast_json_number(object, "wind_gust_mps", &value)) return false;
        result->hours[index].wind_gust_mps = (float)value;
        if (!forecast_json_number(object, "wind_direction_deg", &value)) return false;
        result->hours[index].wind_direction_deg = (uint16_t)value;
        if (!forecast_json_number(object, "weather_code", &value)) return false;
        result->hours[index].weather_code = (uint16_t)value;
        result->hours[index].risk_level = forecast_risk_from_name(hour_risk);
    }
    return true;
}

static void forecast_request_once(void)
{
    settings_snapshot_t settings;
    panel_state_snapshot_t panel;
    forecast_http_response_t response = {0};
    char *request = NULL;
    esp_http_client_handle_t client = NULL;
    int status;

    settings_manager_get_snapshot(&settings);
    panel_state_get_snapshot(&panel);
    if (!forecast_is_configured(&settings)) {
        forecast_state_set_error("forecast is not configured", false);
        return;
    }
    if (!wifi_manager_is_connected()) {
        forecast_state_set_error("Wi-Fi unavailable", true);
        return;
    }
    if (!wifi_manager_is_time_valid()) {
        forecast_state_set_error("waiting for time sync", true);
        return;
    }
    forecast_state_begin(settings.location_name, true);
    request = heap_caps_calloc(1, FORECAST_REQUEST_SIZE, MALLOC_CAP_8BIT);
    response.body = heap_caps_calloc(1, FORECAST_RESPONSE_SIZE, MALLOC_CAP_8BIT);
    response.capacity = FORECAST_RESPONSE_SIZE;
    if (request == NULL || response.body == NULL) {
        forecast_state_set_error("forecast memory unavailable", true);
        goto cleanup;
    }
    int written = snprintf(request, FORECAST_REQUEST_SIZE,
                           "{\"device_id\":\"%s\",\"location_name\":\"%s\",\"latitude\":%.6f,\"longitude\":%.6f,\"timezone\":\"%s\","
                           "\"observation\":{\"wind_speed_mps\":%.1f,\"wind_direction_deg\":%.0f,\"temperature_c\":%.1f,\"humidity_rh\":%.0f,"
                           "\"rain_detected\":%s,\"wind_online\":%s,\"rain_online\":%s}}",
                           settings.device_id, settings.location_name, (double)settings.latitude, (double)settings.longitude,
                           settings.timezone, (double)panel.wind_speed_mps, (double)panel.wind_direction_deg,
                           (double)panel.temperature_c, (double)panel.humidity_rh, panel.rain_detected ? "true" : "false",
                           panel.wind_online ? "true" : "false", panel.rain_online ? "true" : "false");
    if (written <= 0 || (size_t)written >= FORECAST_REQUEST_SIZE) {
        forecast_state_set_error("forecast request too long", true);
        goto cleanup;
    }
    esp_http_client_config_t config = {
        .url = settings.forecast_api_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = forecast_http_event,
        .user_data = &response,
        .buffer_size = 1024,
    };
    client = esp_http_client_init(&config);
    if (client == NULL) { forecast_state_set_error("forecast HTTPS unavailable", true); goto cleanup; }
    esp_http_client_set_header(client, "Content-Type", "application/json");
    // The public Edge endpoint is protected by Cloudflare Browser Integrity Check.
    esp_http_client_set_header(client, "User-Agent", "QXZ-Edge-Panel/1.0");
    esp_http_client_set_header(client, "X-QXZ-Forecast-Key", settings.forecast_key);
    esp_http_client_set_post_field(client, request, written);
    esp_err_t error = esp_http_client_perform(client);
    status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    client = NULL;
    if (error != ESP_OK || response.overflow || status < 200 || status >= 300) {
        char message[64];
        snprintf(message, sizeof(message), "forecast HTTP %d", status);
        forecast_state_set_error(message, true);
        goto cleanup;
    }
    forecast_state_snapshot_t parsed;
    if (!forecast_parse_response(response.body, &settings, &parsed)) {
        forecast_state_set_error("forecast response invalid", true);
        goto cleanup;
    }
    forecast_state_set_result(&parsed);
    ESP_LOGI(TAG, "forecast updated: %s", forecast_risk_name(parsed.next_three_hour_risk));
cleanup:
    if (client != NULL) esp_http_client_cleanup(client);
    heap_caps_free(response.body);
    heap_caps_free(request);
}

static void forecast_task(void *argument)
{
    uint32_t last_request_ms = 0U;
    (void)argument;
    while (true) {
        EventBits_t events = xEventGroupWaitBits(s_events, FORECAST_REQUEST_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
        settings_snapshot_t settings;
        settings_manager_get_snapshot(&settings);
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000LL);
        bool due = wifi_manager_is_connected() && (last_request_ms == 0U ||
                   now - last_request_ms >= settings.forecast_refresh_seconds * 1000U);
        if ((events & FORECAST_REQUEST_BIT) != 0U || due) {
            forecast_request_once();
            last_request_ms = now;
        }
    }
}

esp_err_t forecast_client_init(void)
{
    s_events = xEventGroupCreate();
    if (s_events == NULL) return ESP_ERR_NO_MEM;
    if (xTaskCreate(forecast_task, "forecast", FORECAST_TASK_STACK_SIZE, NULL, 3, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    return ESP_OK;
}

void forecast_client_request_refresh(void)
{
    if (s_events != NULL) xEventGroupSetBits(s_events, FORECAST_REQUEST_BIT);
}
