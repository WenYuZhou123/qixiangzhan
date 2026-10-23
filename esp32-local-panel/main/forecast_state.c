#include "forecast_state.h"

#include <string.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static forecast_state_snapshot_t s_state;
static SemaphoreHandle_t s_lock;

static uint32_t forecast_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

void forecast_state_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_state = (forecast_state_snapshot_t){.status = FORECAST_STATUS_IDLE, .next_three_hour_risk = FORECAST_RISK_UNKNOWN};
}

void forecast_state_begin(const char *location_name, bool configured)
{
    if (s_lock == NULL) return;
    (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    s_state.request_id++;
    if (s_state.request_id == 0U) s_state.request_id = 1U;
    s_state.status = FORECAST_STATUS_LOADING;
    s_state.configured = configured;
    s_state.error[0] = '\0';
    strlcpy(s_state.location_name, location_name != NULL ? location_name : "", sizeof(s_state.location_name));
    s_state.updated_at_ms = forecast_now_ms();
    (void)xSemaphoreGive(s_lock);
}

void forecast_state_set_result(const forecast_state_snapshot_t *result)
{
    if (s_lock == NULL || result == NULL) return;
    (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    uint32_t request_id = s_state.request_id;
    s_state = *result;
    s_state.request_id = request_id;
    s_state.status = FORECAST_STATUS_READY;
    s_state.updated_at_ms = forecast_now_ms();
    (void)xSemaphoreGive(s_lock);
}

void forecast_state_set_error(const char *error, bool configured)
{
    if (s_lock == NULL) return;
    (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    s_state.status = FORECAST_STATUS_ERROR;
    s_state.configured = configured;
    s_state.next_three_hour_risk = FORECAST_RISK_UNKNOWN;
    strlcpy(s_state.error, error != NULL ? error : "forecast unavailable", sizeof(s_state.error));
    s_state.updated_at_ms = forecast_now_ms();
    (void)xSemaphoreGive(s_lock);
}

void forecast_state_get_snapshot(forecast_state_snapshot_t *snapshot)
{
    if (snapshot == NULL || s_lock == NULL) return;
    (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    *snapshot = s_state;
    (void)xSemaphoreGive(s_lock);
}

bool forecast_state_is_current(const forecast_state_snapshot_t *snapshot)
{
    uint32_t now = forecast_now_ms();
    return snapshot != NULL && snapshot->status == FORECAST_STATUS_READY &&
           snapshot->expires_at_ms != 0U && (int32_t)(snapshot->expires_at_ms - now) > 0;
}

const char *forecast_risk_name(forecast_risk_level_t level)
{
    switch (level) {
    case FORECAST_RISK_NORMAL: return "normal";
    case FORECAST_RISK_CAUTION: return "caution";
    case FORECAST_RISK_DANGER: return "danger";
    default: return "unknown";
    }
}

forecast_risk_level_t forecast_risk_from_name(const char *name)
{
    if (name != NULL && strcmp(name, "normal") == 0) return FORECAST_RISK_NORMAL;
    if (name != NULL && strcmp(name, "caution") == 0) return FORECAST_RISK_CAUTION;
    if (name != NULL && strcmp(name, "danger") == 0) return FORECAST_RISK_DANGER;
    return FORECAST_RISK_UNKNOWN;
}
