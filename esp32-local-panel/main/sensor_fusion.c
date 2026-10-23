#include "sensor_fusion.h"

sensor_fusion_result_t sensor_fusion_evaluate(const panel_state_snapshot_t *state)
{
    sensor_fusion_result_t result = {
        .level = SENSOR_RISK_UNKNOWN,
        .data_valid = false,
        .recommend_control = false,
        .reason = "sensor data unavailable",
    };

    if (state == NULL || !state->h743_online || !state->status_valid ||
        !state->wind_online || !state->rain_online) {
        return result;
    }

    result.data_valid = true;
    if (state->rain_detected) {
        result.level = SENSOR_RISK_DANGER;
        result.recommend_control = true;
        result.reason = "rain detected";
    } else if (state->wind_speed_mps >= 12.0f) {
        result.level = SENSOR_RISK_DANGER;
        result.recommend_control = true;
        result.reason = "wind speed at or above 12 m/s";
    } else if (state->wind_speed_mps >= 8.0f) {
        result.level = SENSOR_RISK_CAUTION;
        result.reason = "wind speed from 8 to 12 m/s";
    } else {
        result.level = SENSOR_RISK_NORMAL;
        result.reason = "wind and rain are within demo thresholds";
    }
    return result;
}

const char *sensor_fusion_risk_name(sensor_risk_level_t level)
{
    switch (level) {
    case SENSOR_RISK_NORMAL:
        return "normal";
    case SENSOR_RISK_CAUTION:
        return "caution";
    case SENSOR_RISK_DANGER:
        return "danger";
    default:
        return "unknown";
    }
}
