#include "ui_theme.h"

#include <stdio.h>

LV_FONT_DECLARE(panel_zh_20);

void ui_theme_format_decimal(char *destination, size_t destination_size, float value, uint8_t decimals)
{
    static const uint32_t scales[] = {1U, 10U, 100U, 1000U};
    uint32_t scale;
    int64_t scaled;
    uint64_t magnitude;
    uint64_t integer_part;
    uint64_t fraction_part;
    const char *sign;

    if (destination == NULL || destination_size == 0U) {
        return;
    }
    if (decimals > 3U) {
        decimals = 3U;
    }
    scale = scales[decimals];
    scaled = (int64_t)(value * (float)scale + (value < 0.0f ? -0.5f : 0.5f));
    sign = scaled < 0 ? "-" : "";
    magnitude = scaled < 0 ? (uint64_t)(-scaled) : (uint64_t)scaled;
    integer_part = magnitude / scale;
    fraction_part = magnitude % scale;

    if (decimals == 0U) {
        (void)snprintf(destination, destination_size, "%s%llu", sign, (unsigned long long)integer_part);
    } else if (decimals == 1U) {
        (void)snprintf(destination, destination_size, "%s%llu.%01llu", sign,
                       (unsigned long long)integer_part, (unsigned long long)fraction_part);
    } else if (decimals == 2U) {
        (void)snprintf(destination, destination_size, "%s%llu.%02llu", sign,
                       (unsigned long long)integer_part, (unsigned long long)fraction_part);
    } else {
        (void)snprintf(destination, destination_size, "%s%llu.%03llu", sign,
                       (unsigned long long)integer_part, (unsigned long long)fraction_part);
    }
}

lv_obj_t *ui_theme_label_create(lv_obj_t *parent, const char *text, int x, int y, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    lv_obj_set_pos(label, x, y);
    return label;
}

lv_obj_t *ui_theme_card_create(lv_obj_t *parent, int x, int y, int width, int height, const char *title)
{
    lv_obj_t *card = lv_obj_create(parent);

    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, width, height);
    lv_obj_set_style_radius(card, UI_CARD_RADIUS, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(UI_SURFACE_COLOR), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(UI_BORDER_COLOR), 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    ui_theme_label_create(card, title, 14, 10, &panel_zh_20, lv_color_hex(UI_MUTED_COLOR));
    return card;
}

lv_obj_t *ui_theme_value_label_create(lv_obj_t *parent, int x, int y, int width)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_width(label, width);
    lv_obj_set_style_text_font(label, &panel_zh_20, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(UI_TEXT_COLOR), 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    lv_obj_set_pos(label, x, y);
    return label;
}
