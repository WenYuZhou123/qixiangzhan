#pragma once

#include "lvgl.h"
#include "panel_state.h"

typedef struct {
    lv_obj_t *card;
    lv_obj_t *chart;
    lv_chart_series_t *series;
    lv_obj_t *metric_labels[5];
    lv_obj_t *value_labels[5];
    lv_obj_t *reference_label;
    lv_obj_t *offline_label;
} ui_air_quality_t;

void ui_air_quality_init(ui_air_quality_t *view, lv_obj_t *parent, int x, int y, int width, int height);
void ui_air_quality_update(ui_air_quality_t *view, const panel_state_snapshot_t *state);

