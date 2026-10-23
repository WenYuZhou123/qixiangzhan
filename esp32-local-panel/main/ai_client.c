#include "ai_client.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "ai_state.h"
#include "forecast_state.h"
#include "panel_state.h"
#include "sensor_fusion.h"
#include "settings_manager.h"
#include "wifi_manager.h"

static const char *TAG = "ai_client";
static const EventBits_t AI_REQUEST_BIT = BIT0;
#define AI_REQUEST_BODY_SIZE 2048U
#define AI_RESPONSE_BODY_SIZE 4096U
#define AI_DIRECT_CONTENT_SIZE 3072U
#define AI_TASK_STACK_SIZE 12288U
static EventGroupHandle_t s_events;

typedef struct {
    char *body;
    size_t capacity;
    size_t length;
    bool overflow;
} ai_http_response_t;

static bool ai_client_is_direct_model(const settings_snapshot_t *settings)
{
    static const char *suffix = "/chat/completions";
    size_t length = strlen(settings->edge_api_url);
    return length >= strlen(suffix) &&
           strcmp(&settings->edge_api_url[length - strlen(suffix)], suffix) == 0;
}

static panel_command_t ai_client_command_from_name(const char *name)
{
    if (name == NULL) {
        return PANEL_COMMAND_NONE;
    }
    if (strcmp(name, "pad_open") == 0) {
        return PANEL_COMMAND_PAD_OPEN;
    }
    if (strcmp(name, "pad_close") == 0) {
        return PANEL_COMMAND_PAD_CLOSE;
    }
    if (strcmp(name, "pad_stop") == 0) {
        return PANEL_COMMAND_PAD_STOP;
    }
    if (strcmp(name, "query_status") == 0) {
        return PANEL_COMMAND_QUERY_STATUS;
    }
    return PANEL_COMMAND_NONE;
}

static esp_err_t ai_client_http_event(esp_http_client_event_t *event)
{
    ai_http_response_t *response = event->user_data;
    if (event->event_id != HTTP_EVENT_ON_DATA || response == NULL || event->data == NULL) {
        return ESP_OK;
    }
    if (response->body == NULL || response->length + event->data_len >= response->capacity) {
        response->overflow = true;
        return ESP_FAIL;
    }
    memcpy(&response->body[response->length], event->data, event->data_len);
    response->length += event->data_len;
    response->body[response->length] = '\0';
    return ESP_OK;
}

static const char *ai_client_bool(bool value)
{
    return value ? "true" : "false";
}

static bool ai_client_build_edge_request(char *destination, size_t destination_size,
                                         const panel_state_snapshot_t *state,
                                         const sensor_fusion_result_t *fusion,
                                         const settings_snapshot_t *settings,
                                         const forecast_state_snapshot_t *forecast)
{
    int written = snprintf(destination, destination_size,
                           "{\"device_id\":\"%s\",\"snapshot_id\":%u,\"captured_uptime_ms\":%lu,\"status_age_ms\":%lu,"
                           "\"relays\":{\"left_on\":%s,\"right_on\":%s},"
                           "\"weather\":{\"wind_speed_mps\":%.1f,\"wind_direction_deg\":%.1f,\"temperature_c\":%.2f,\"humidity_rh\":%.1f,"
                           "\"rain_detected\":%s,\"rain_value\":%.1f,\"pm25_ugm3\":%.1f,\"pm10_ugm3\":%.1f,\"co2_ppm\":%.0f,"
                           "\"tvoc_mg_m3\":%.3f,\"ch2o_mg_m3\":%.3f,\"capabilities\":{\"wind\":%s,\"air\":%s,\"rain\":%s}},"
                           "\"fusion\":{\"risk_level\":\"%s\",\"data_valid\":%s,\"reason\":\"%s\"},"
                           "\"forecast\":{\"available\":%s,\"next_three_hour_risk\":\"%s\",\"local_correction_applied\":%s,\"hours\":["
                           "{\"wind_speed_mps\":%.1f,\"wind_gust_mps\":%.1f,\"precipitation_probability\":%u},"
                           "{\"wind_speed_mps\":%.1f,\"wind_gust_mps\":%.1f,\"precipitation_probability\":%u},"
                           "{\"wind_speed_mps\":%.1f,\"wind_gust_mps\":%.1f,\"precipitation_probability\":%u}]}}",
                           settings->device_id, state->last_sequence,
                           (unsigned long)state->device_uptime_ms, (unsigned long)state->last_update_age_ms,
                           ai_client_bool(state->relay1_on), ai_client_bool(state->relay2_on),
                           (double)state->wind_speed_mps, (double)state->wind_direction_deg,
                           (double)state->temperature_c, (double)state->humidity_rh,
                           ai_client_bool(state->rain_detected), (double)state->rain_value,
                           (double)state->pm25_ugm3, (double)state->pm10_ugm3, (double)state->co2_ppm,
                           (double)state->tvoc_mg_m3, (double)state->ch2o_mg_m3,
                           ai_client_bool(state->wind_online), ai_client_bool(state->air_online),
                           ai_client_bool(state->rain_online), sensor_fusion_risk_name(fusion->level),
                           ai_client_bool(fusion->data_valid), fusion->reason,
                           ai_client_bool(forecast_state_is_current(forecast)), forecast_risk_name(forecast->next_three_hour_risk),
                           ai_client_bool(forecast->local_correction_applied),
                           (double)forecast->hours[0].wind_speed_mps, (double)forecast->hours[0].wind_gust_mps,
                           forecast->hours[0].precipitation_probability,
                           (double)forecast->hours[1].wind_speed_mps, (double)forecast->hours[1].wind_gust_mps,
                           forecast->hours[1].precipitation_probability,
                           (double)forecast->hours[2].wind_speed_mps, (double)forecast->hours[2].wind_gust_mps,
                           forecast->hours[2].precipitation_probability);
    return written > 0 && (size_t)written < destination_size;
}

static bool ai_client_build_direct_request(char *destination, size_t destination_size,
                                           const panel_state_snapshot_t *state,
                                           const sensor_fusion_result_t *fusion,
                                           const settings_snapshot_t *settings,
                                           const forecast_state_snapshot_t *forecast)
{
    int written = snprintf(destination, destination_size,
                           "{\"model\":\"%s\",\"stream\":false,\"enable_thinking\":false,"
                           "\"response_format\":{\"type\":\"json_object\"},\"temperature\":0.1,\"max_tokens\":160,"
                           "\"messages\":[{\"role\":\"system\",\"content\":\"You are an airport safety assistant. "
                           "Return exactly one JSON object, no markdown. Keys: risk_level, summary, reasons, "
                           "recommended_action, requires_confirmation. risk_level is normal, caution, danger, or unknown. "
                           "recommended_action is none, pad_open, pad_close, pad_stop, or query_status. "
                           "requires_confirmation must be true. Never claim offline data is valid. "
                           "Use only these Chinese phrases for summary and reasons: "
                           "气象条件正常，当前无高风险;风速处于安全范围;未检测到降雨;传感器在线且数据有效;"
                           "风速偏高，请关注停机坪状态;风速处于关注范围;请确认现场条件;建议持续监测数据;"
                           "风雨风险较高，建议停止操作;风速达到风险阈值;检测到降雨信号;建议人工确认后停止;"
                           "传感器数据不完整，暂不建议操作;风速或雨滴数据离线;无法确认现场条件;请检查传感器链路.\"},"
                           "{\"role\":\"user\",\"content\":\"wind=%.1f m/s; direction=%.0f deg; rain=%s; "
                           "temperature=%.1f C; humidity=%.0f%%; wind_online=%s; rain_online=%s; fusion=%s; "
                           "fusion_valid=%s; forecast_available=%s; forecast_risk=%s; forecast_0_wind=%.1f; forecast_0_gust=%.1f; forecast_0_rain_probability=%u.\"}]}",
                           settings->llm_model, (double)state->wind_speed_mps,
                           (double)state->wind_direction_deg, state->rain_detected ? "true" : "false",
                           (double)state->temperature_c, (double)state->humidity_rh,
                           ai_client_bool(state->wind_online), ai_client_bool(state->rain_online),
                           sensor_fusion_risk_name(fusion->level), ai_client_bool(fusion->data_valid),
                           ai_client_bool(forecast_state_is_current(forecast)), forecast_risk_name(forecast->next_three_hour_risk),
                           (double)forecast->hours[0].wind_speed_mps, (double)forecast->hours[0].wind_gust_mps,
                           forecast->hours[0].precipitation_probability);
    return written > 0 && (size_t)written < destination_size;
}

static bool ai_client_json_string(const char *body, const char *key, char *destination, size_t destination_size)
{
    char token[48];
    const char *value;
    const char *end;
    int written = snprintf(token, sizeof(token), "\"%s\"", key);
    if (written <= 0 || (size_t)written >= sizeof(token) || destination_size == 0U) {
        return false;
    }
    value = strstr(body, token);
    if (value == NULL || (value = strchr(value + strlen(token), ':')) == NULL) {
        return false;
    }
    value = strchr(value, '"');
    if (value == NULL || (end = strchr(++value, '"')) == NULL || (size_t)(end - value) >= destination_size) {
        return false;
    }
    memcpy(destination, value, (size_t)(end - value));
    destination[end - value] = '\0';
    return true;
}

static bool ai_client_json_true(const char *body, const char *key)
{
    char token[48];
    const char *value;
    int written = snprintf(token, sizeof(token), "\"%s\"", key);
    if (written <= 0 || (size_t)written >= sizeof(token)) {
        return false;
    }
    value = strstr(body, token);
    if (value == NULL || (value = strchr(value + strlen(token), ':')) == NULL) {
        return false;
    }
    while (*++value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') {}
    return strncmp(value, "true", 4) == 0;
}

static bool ai_client_extract_openai_content(const char *body, char *destination, size_t destination_size)
{
    const char *value = strstr(body, "\"content\"");
    size_t length = 0U;

    if (value == NULL || (value = strchr(value + 9, ':')) == NULL || destination_size == 0U) {
        return false;
    }
    while (*++value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') {}
    if (*value++ != '\"') {
        return false;
    }
    while (*value != '\0' && *value != '\"') {
        char character = *value++;
        if (character == '\\') {
            character = *value++;
            if (character == 'n') character = '\n';
            else if (character == 'r') character = '\r';
            else if (character == 't') character = '\t';
            else if (character != '\"' && character != '\\' && character != '/') return false;
        }
        if (character == '\0' || length + 1U >= destination_size) {
            return false;
        }
        destination[length++] = character;
    }
    destination[length] = '\0';
    return *value == '\"';
}

static void ai_client_apply_response(const char *body, const sensor_fusion_result_t *fusion,
                                     const forecast_state_snapshot_t *forecast)
{
    char summary[160];
    char action[32];
    char reason_storage[3][112] = {{0}};
    const char *reason_texts[3] = {reason_storage[0], reason_storage[1], reason_storage[2]};
    const char *reasons;
    const char *cursor;

    if (!ai_client_json_string(body, "summary", summary, sizeof(summary)) ||
        !ai_client_json_string(body, "recommended_action", action, sizeof(action)) ||
        !ai_client_json_true(body, "requires_confirmation") ||
        (reasons = strstr(body, "\"reasons\"")) == NULL) {
        ai_state_set_error(*fusion, "AI response schema invalid");
        return;
    }
    cursor = strchr(reasons, '[');
    for (size_t index = 0; cursor != NULL && index < 3U; index++) {
        const char *start = strchr(cursor, '"');
        const char *end;
        if (start == NULL || (end = strchr(++start, '"')) == NULL ||
            (size_t)(end - start) >= sizeof(reason_storage[index])) {
            break;
        }
        memcpy(reason_storage[index], start, (size_t)(end - start));
        reason_storage[index][end - start] = '\0';
        cursor = end + 1;
    }
    panel_command_t command = ai_client_command_from_name(action);
    if (forecast_state_is_current(forecast)) {
        if (command == PANEL_COMMAND_PAD_OPEN) {
            command = PANEL_COMMAND_QUERY_STATUS;
        }
        if (forecast->next_three_hour_risk == FORECAST_RISK_DANGER &&
            command != PANEL_COMMAND_PAD_CLOSE && command != PANEL_COMMAND_PAD_STOP &&
            command != PANEL_COMMAND_QUERY_STATUS) {
            command = PANEL_COMMAND_QUERY_STATUS;
        }
    }
    ai_state_set_result(*fusion, command, true, summary, reason_texts);
}

static void ai_client_set_transport_error(sensor_fusion_result_t fusion, esp_err_t error, int status, bool response_overflow)
{
    char message[48];

    if (response_overflow) {
        ai_state_set_error(fusion, "AI response too long");
        return;
    }
    if (error != ESP_OK) {
        (void)snprintf(message, sizeof(message), "HTTPS %s", esp_err_to_name(error));
        ai_state_set_error(fusion, message);
        return;
    }
    (void)snprintf(message, sizeof(message), "AI HTTP %d", status);
    ai_state_set_error(fusion, message);
}

static void ai_client_analyze(void)
{
    panel_state_snapshot_t panel;
    sensor_fusion_result_t fusion;
    settings_snapshot_t settings;
    forecast_state_snapshot_t forecast;
    ai_http_response_t response = {0};
    esp_http_client_handle_t client = NULL;
    char *request_body = NULL;
    char *authorization = NULL;
    char *direct_content = NULL;
    int status = 0;
    esp_err_t error;
    bool direct_model;

    panel_state_get_snapshot(&panel);
    settings_manager_get_snapshot(&settings);
    forecast_state_get_snapshot(&forecast);
    fusion = sensor_fusion_evaluate(&panel);
    if (!fusion.data_valid) {
        ai_state_set_error(fusion, "wind or rain data unavailable");
        return;
    }
    if (!wifi_manager_is_connected()) {
        ai_state_set_error(fusion, "Wi-Fi unavailable");
        return;
    }
    if (!wifi_manager_is_time_valid()) {
        ai_state_set_error(fusion, "waiting for time sync");
        return;
    }
    if (settings.edge_api_url[0] == '\0' || settings.edge_key[0] == '\0') {
        ai_state_set_error(fusion, "AI API is not configured");
        return;
    }
    direct_model = ai_client_is_direct_model(&settings);
    if (direct_model && settings.llm_model[0] == '\0') {
        ai_state_set_error(fusion, "direct model is not configured");
        return;
    }

    ai_state_begin(panel.last_sequence, fusion);
    request_body = heap_caps_calloc(1U, AI_REQUEST_BODY_SIZE, MALLOC_CAP_8BIT);
    response.body = heap_caps_calloc(1U, AI_RESPONSE_BODY_SIZE, MALLOC_CAP_8BIT);
    response.capacity = AI_RESPONSE_BODY_SIZE;
    if (direct_model) {
        direct_content = heap_caps_calloc(1U, AI_DIRECT_CONTENT_SIZE, MALLOC_CAP_8BIT);
        authorization = heap_caps_calloc(1U, SETTINGS_EDGE_KEY_MAX + 8U, MALLOC_CAP_8BIT);
    }
    if (request_body == NULL || response.body == NULL ||
        (direct_model && (direct_content == NULL || authorization == NULL))) {
        ESP_LOGE(TAG, "insufficient heap for AI request");
        ai_state_set_error(fusion, "AI memory unavailable");
        goto cleanup;
    }

    if (!(direct_model ?
          ai_client_build_direct_request(request_body, AI_REQUEST_BODY_SIZE, &panel, &fusion, &settings, &forecast) :
          ai_client_build_edge_request(request_body, AI_REQUEST_BODY_SIZE, &panel, &fusion, &settings, &forecast))) {
        ai_state_set_error(fusion, "cannot create AI request");
        goto cleanup;
    }

    const esp_http_client_config_t config = {
        .url = settings.edge_api_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = CONFIG_PANEL_AI_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = ai_client_http_event,
        .user_data = &response,
        .buffer_size = 1024,
    };
    client = esp_http_client_init(&config);
    if (client == NULL) {
        ai_state_set_error(fusion, "cannot create HTTPS client");
        goto cleanup;
    }
    (void)esp_http_client_set_header(client, "Content-Type", "application/json");
    // Cloudflare Browser Integrity Check rejects requests with no user agent.
    (void)esp_http_client_set_header(client, "User-Agent", "QXZ-Edge-Panel/1.0");
    if (direct_model) {
        (void)snprintf(authorization, SETTINGS_EDGE_KEY_MAX + 8U, "Bearer %s", settings.edge_key);
        (void)esp_http_client_set_header(client, "Authorization", authorization);
        ESP_LOGI(TAG, "direct model request: %s", settings.llm_model);
    } else {
        (void)esp_http_client_set_header(client, "X-QXZ-Edge-Key", settings.edge_key);
        ESP_LOGI(TAG, "edge proxy request");
    }
    (void)esp_http_client_set_post_field(client, request_body, strlen(request_body));
    error = esp_http_client_perform(client);
    status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    client = NULL;

    if (error != ESP_OK || response.overflow) {
        ESP_LOGW(TAG, "AI request failed: error=%s, response_overflow=%d", esp_err_to_name(error), response.overflow);
        ai_client_set_transport_error(fusion, error, status, response.overflow);
    } else if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "AI endpoint returned HTTP %d: %s", status, response.body);
        ai_client_set_transport_error(fusion, error, status, false);
    } else {
        if (direct_model) {
            if (ai_client_extract_openai_content(response.body, direct_content, AI_DIRECT_CONTENT_SIZE)) {
                ai_client_apply_response(direct_content, &fusion, &forecast);
            } else {
                ESP_LOGW(TAG, "direct model response did not contain content");
                ai_state_set_error(fusion, "model response schema invalid");
            }
        } else {
            ai_client_apply_response(response.body, &fusion, &forecast);
        }
    }

cleanup:
    if (client != NULL) {
        esp_http_client_cleanup(client);
    }
    heap_caps_free(direct_content);
    heap_caps_free(authorization);
    heap_caps_free(response.body);
    heap_caps_free(request_body);
    ESP_LOGI(TAG, "AI task stack remaining: %u bytes", (unsigned)uxTaskGetStackHighWaterMark(NULL));
}

static void ai_client_task(void *argument)
{
    uint32_t last_auto_request = 0U;
    (void)argument;
    while (true) {
        EventBits_t events = xEventGroupWaitBits(s_events, AI_REQUEST_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000LL);
        settings_snapshot_t settings;
        settings_manager_get_snapshot(&settings);
        bool due = wifi_manager_is_connected() && last_auto_request != 0U &&
                   now - last_auto_request >= settings.ai_auto_interval_seconds * 1000U;
        if ((events & AI_REQUEST_BIT) != 0U || due) {
            ai_client_analyze();
            last_auto_request = now;
        } else if (wifi_manager_is_connected() && last_auto_request == 0U) {
            last_auto_request = now;
        }
    }
}

esp_err_t ai_client_init(void)
{
    s_events = xEventGroupCreate();
    if (s_events == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(ai_client_task, "ai_client", AI_TASK_STACK_SIZE, NULL, 4, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void ai_client_request_analysis(void)
{
    if (s_events != NULL) {
        xEventGroupSetBits(s_events, AI_REQUEST_BIT);
    }
}
