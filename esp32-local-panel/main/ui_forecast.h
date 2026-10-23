#pragma once

#include "lvgl.h"

#include "forecast_state.h"

typedef struct {
    lv_obj_t *status;
    lv_obj_t *location;
    lv_obj_t *risk;
    lv_obj_t *detail;
    lv_obj_t *columns[FORECAST_HOUR_COUNT];
    lv_obj_t *wind_bars[FORECAST_HOUR_COUNT];
    lv_obj_t *gust_bars[FORECAST_HOUR_COUNT];
    lv_obj_t *rain_bars[FORECAST_HOUR_COUNT];
    lv_obj_t *time_labels[FORECAST_HOUR_COUNT];
    uint8_t selected_hour;
} ui_forecast_t;

void ui_forecast_init(ui_forecast_t *view, lv_obj_t *parent, lv_event_cb_t refresh_callback, void *refresh_user_data);
void ui_forecast_update(ui_forecast_t *view, const forecast_state_snapshot_t *state);
