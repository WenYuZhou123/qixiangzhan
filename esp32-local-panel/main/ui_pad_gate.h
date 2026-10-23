#pragma once

#include <stdbool.h>

#include "lvgl.h"
#include "panel_state.h"

typedef struct {
    lv_obj_t *card;
    lv_obj_t *left_door;
    lv_obj_t *right_door;
    lv_obj_t *state_label;
    lv_obj_t *channel_label;
    lv_obj_t *flash_strip;
    int8_t pose;
    uint32_t observed_completion_id;
} ui_pad_gate_t;

void ui_pad_gate_init(ui_pad_gate_t *view, lv_obj_t *parent, int x, int y, int width, int height);
void ui_pad_gate_update(ui_pad_gate_t *view, const panel_state_snapshot_t *state);
bool ui_pad_gate_is_pending(const panel_state_snapshot_t *state);

