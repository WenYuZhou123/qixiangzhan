#pragma once

#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

#define UI_BG_COLOR          0x242526
#define UI_SURFACE_COLOR     0x303234
#define UI_SURFACE_MUTED     0x3C3F42
#define UI_TEXT_COLOR        0xEEF1F2
#define UI_MUTED_COLOR       0xA8AFB4
#define UI_ACCENT_COLOR      0x1689C9
#define UI_ONLINE_COLOR      0x2AAE8A
#define UI_WARNING_COLOR     0xE0A53A
#define UI_DANGER_COLOR      0xDE5757
#define UI_BORDER_COLOR      0x5A5E61
#define UI_CARD_RADIUS       8

lv_obj_t *ui_theme_card_create(lv_obj_t *parent, int x, int y, int width, int height, const char *title);
lv_obj_t *ui_theme_label_create(lv_obj_t *parent, const char *text, int x, int y, const lv_font_t *font, lv_color_t color);
lv_obj_t *ui_theme_value_label_create(lv_obj_t *parent, int x, int y, int width);
void ui_theme_format_decimal(char *destination, size_t destination_size, float value, uint8_t decimals);
