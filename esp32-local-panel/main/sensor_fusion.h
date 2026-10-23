#pragma once

#include <stdbool.h>

#include "panel_state.h"

typedef enum {
    SENSOR_RISK_UNKNOWN = 0,
    SENSOR_RISK_NORMAL,
    SENSOR_RISK_CAUTION,
    SENSOR_RISK_DANGER,
} sensor_risk_level_t;

typedef struct {
    sensor_risk_level_t level;
    bool data_valid;
    bool recommend_control;
    const char *reason;
} sensor_fusion_result_t;

sensor_fusion_result_t sensor_fusion_evaluate(const panel_state_snapshot_t *state);
const char *sensor_fusion_risk_name(sensor_risk_level_t level);
