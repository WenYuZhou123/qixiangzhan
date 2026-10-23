#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FORECAST_HOUR_COUNT 12U

typedef enum {
    FORECAST_STATUS_IDLE = 0,
    FORECAST_STATUS_LOADING,
    FORECAST_STATUS_READY,
    FORECAST_STATUS_ERROR,
} forecast_status_t;

typedef enum {
    FORECAST_RISK_NORMAL = 0,
    FORECAST_RISK_CAUTION,
    FORECAST_RISK_DANGER,
    FORECAST_RISK_UNKNOWN,
} forecast_risk_level_t;

typedef struct {
    char time[24];
    float temperature_c;
    uint8_t humidity_rh;
    uint8_t precipitation_probability;
    float precipitation_mm;
    float wind_speed_mps;
    float wind_gust_mps;
    uint16_t wind_direction_deg;
    uint16_t weather_code;
    forecast_risk_level_t risk_level;
} forecast_hour_t;

typedef struct {
    forecast_status_t status;
    forecast_risk_level_t next_three_hour_risk;
    bool configured;
    bool local_correction_applied;
    uint32_t request_id;
    uint32_t updated_at_ms;
    uint32_t expires_at_ms;
    char source[20];
    char location_name[65];
    char error[96];
    forecast_hour_t hours[FORECAST_HOUR_COUNT];
} forecast_state_snapshot_t;

void forecast_state_init(void);
void forecast_state_begin(const char *location_name, bool configured);
void forecast_state_set_result(const forecast_state_snapshot_t *result);
void forecast_state_set_error(const char *error, bool configured);
void forecast_state_get_snapshot(forecast_state_snapshot_t *snapshot);
bool forecast_state_is_current(const forecast_state_snapshot_t *snapshot);
const char *forecast_risk_name(forecast_risk_level_t level);
forecast_risk_level_t forecast_risk_from_name(const char *name);
