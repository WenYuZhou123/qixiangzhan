#pragma once

#include "lvgl.h"

#include "ai_state.h"
#include "panel_state.h"
#include "wifi_manager.h"

typedef struct ui_settings ui_settings_t;

ui_settings_t *ui_settings_create(lv_obj_t *parent);
bool ui_settings_needs_refresh(const ui_settings_t *view);
void ui_settings_update(ui_settings_t *view, const panel_state_snapshot_t *panel,
                        const wifi_manager_snapshot_t *wifi, const ai_state_snapshot_t *ai);
