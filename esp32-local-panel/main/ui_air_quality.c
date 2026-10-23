#include "ui_air_quality.h"

#include <stdio.h>

#include "ui_theme.h"

LV_FONT_DECLARE(panel_zh_20);

static const char *const s_metric_names[] = { "PM2.5", "PM10", "CO2", "TVOC", "CH2O" };
static const float s_reference_values[] = { 35.0f, 50.0f, 1000.0f, 0.60f, 0.08f };

static lv_color_t ui_air_quality_color(int32_t percent)
{
    if (percent > 100) {
        return lv_color_hex(UI_DANGER_COLOR);
    }
    if (percent >= 70) {
        return lv_color_hex(UI_WARNING_COLOR);
    }
    return lv_color_hex(UI_ACCENT_COLOR);
}

static void ui_air_quality_chart_draw(lv_event_t *event)
{
    lv_obj_draw_part_dsc_t *draw = lv_event_get_param(event);
    ui_air_quality_t *view = lv_event_get_user_data(event);

    if (draw == NULL || view == NULL || draw->part != LV_PART_ITEMS || draw->rect_dsc == NULL || draw->id >= 5U) {
        return;
    }
    draw->rect_dsc->bg_color = ui_air_quality_color(view->series->y_points[draw->id]);
}

static int32_t ui_air_quality_percent(float value, float reference)
{
    int32_t percent = (int32_t)((value * 100.0f / reference) + 0.5f);

    if (percent < 0) {
        return 0;
    }
    return (percent > 150) ? 150 : percent;
}

void ui_air_quality_init(ui_air_quality_t *view, lv_obj_t *parent, int x, int y, int width, int height)
{
    static const char *const chart_labels[] = { "PM2.5", "PM10", "CO2", "TVOC", "CH2O" };
    int index;

    view->card = ui_theme_card_create(parent, x, y, width, height, "空气质量");
    view->chart = lv_chart_create(view->card);
    lv_obj_set_pos(view->chart, 14, 44);
    lv_obj_set_size(view->chart, 224, 166);
    lv_chart_set_type(view->chart, LV_CHART_TYPE_BAR);
    lv_chart_set_range(view->chart, LV_CHART_AXIS_PRIMARY_Y, 0, 150);
    lv_chart_set_point_count(view->chart, 5);
    lv_chart_set_div_line_count(view->chart, 3, 0);
    lv_obj_set_style_bg_color(view->chart, lv_color_hex(UI_SURFACE_MUTED), 0);
    lv_obj_set_style_border_width(view->chart, 0, 0);
    lv_obj_set_style_line_color(view->chart, lv_color_hex(UI_BORDER_COLOR), LV_PART_MAIN);
    lv_obj_set_style_pad_column(view->chart, 8, LV_PART_ITEMS);
    view->series = lv_chart_add_series(view->chart, lv_color_hex(UI_ACCENT_COLOR), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_add_event_cb(view->chart, ui_air_quality_chart_draw, LV_EVENT_DRAW_PART_BEGIN, view);

    for (index = 0; index < 5; index++) {
        view->metric_labels[index] = ui_theme_label_create(view->card,
                                                            chart_labels[index],
                                                            18 + index * 43,
                                                            214,
                                                            &lv_font_montserrat_14,
                                                            lv_color_hex(UI_MUTED_COLOR));
        view->value_labels[index] = ui_theme_label_create(view->card,
                                                           s_metric_names[index],
                                                           252,
                                                           42 + index * 39,
                                                           &panel_zh_20,
                                                           lv_color_hex(UI_TEXT_COLOR));
    }
    view->reference_label = ui_theme_label_create(view->card, "参考阈值 100%", 14, height - 30, &panel_zh_20, lv_color_hex(UI_MUTED_COLOR));
    view->offline_label = ui_theme_label_create(view->card, "传感器离线", 70, 110, &panel_zh_20, lv_color_hex(UI_DANGER_COLOR));
    lv_obj_add_flag(view->offline_label, LV_OBJ_FLAG_HIDDEN);
}

void ui_air_quality_update(ui_air_quality_t *view, const panel_state_snapshot_t *state)
{
    float values[5];
    char text[24];
    int index;

    if (!state->air_online) {
        lv_obj_add_flag(view->chart, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->offline_label, LV_OBJ_FLAG_HIDDEN);
        for (index = 0; index < 5; index++) {
            lv_obj_add_flag(view->metric_labels[index], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(view->value_labels[index], LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_add_flag(view->reference_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    values[0] = state->pm25_ugm3;
    values[1] = state->pm10_ugm3;
    values[2] = state->co2_ppm;
    values[3] = state->tvoc_mg_m3;
    values[4] = state->ch2o_mg_m3;
    lv_obj_clear_flag(view->chart, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view->offline_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(view->reference_label, LV_OBJ_FLAG_HIDDEN);
    for (index = 0; index < 5; index++) {
        lv_obj_clear_flag(view->metric_labels[index], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->value_labels[index], LV_OBJ_FLAG_HIDDEN);
        lv_chart_set_value_by_id(view->chart, view->series, index, ui_air_quality_percent(values[index], s_reference_values[index]));
    }
    lv_chart_refresh(view->chart);
    ui_theme_format_decimal(text, sizeof(text), values[0], 0U);
    lv_label_set_text_fmt(view->value_labels[0], "%s ug/m3", text);
    ui_theme_format_decimal(text, sizeof(text), values[1], 0U);
    lv_label_set_text_fmt(view->value_labels[1], "%s ug/m3", text);
    ui_theme_format_decimal(text, sizeof(text), values[2], 0U);
    lv_label_set_text_fmt(view->value_labels[2], "%s ppm", text);
    ui_theme_format_decimal(text, sizeof(text), values[3], 3U);
    lv_label_set_text_fmt(view->value_labels[3], "%s mg/m3", text);
    ui_theme_format_decimal(text, sizeof(text), values[4], 3U);
    lv_label_set_text_fmt(view->value_labels[4], "%s mg/m3", text);
}
