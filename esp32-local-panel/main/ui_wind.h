#pragma once

#include "lvgl.h"
#include "panel_state.h"

typedef struct {
    lv_obj_t *card;
    lv_obj_t *needle;
    lv_point_t needle_points[2];
    lv_obj_t *wind_lines[3];
    lv_obj_t *speed_label;
    lv_obj_t *direction_label;
    lv_obj_t *offline_label;
} ui_wind_t;

void ui_wind_init(ui_wind_t *view, lv_obj_t *parent, int x, int y, int width, int height);
void ui_wind_update(ui_wind_t *view, const panel_state_snapshot_t *state);

