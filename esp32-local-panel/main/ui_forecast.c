#include "ui_forecast.h"

#include <stdio.h>
#include <string.h>

#include "ui_theme.h"

LV_FONT_DECLARE(panel_zh_16);
LV_FONT_DECLARE(panel_zh_20);

static lv_color_t ui_forecast_risk_color(forecast_risk_level_t risk)
{
    if (risk == FORECAST_RISK_DANGER) return lv_color_hex(UI_DANGER_COLOR);
    if (risk == FORECAST_RISK_CAUTION) return lv_color_hex(UI_WARNING_COLOR);
    if (risk == FORECAST_RISK_NORMAL) return lv_color_hex(UI_ONLINE_COLOR);
    return lv_color_hex(UI_MUTED_COLOR);
}

static const char *ui_forecast_risk_text(forecast_risk_level_t risk)
{
    switch (risk) {
    case FORECAST_RISK_NORMAL: return "正常";
    case FORECAST_RISK_CAUTION: return "关注";
    case FORECAST_RISK_DANGER: return "高风险";
    default: return "不可用";
    }
}

static void ui_forecast_hour_event(lv_event_t *event)
{
    ui_forecast_t *view = lv_event_get_user_data(event);
    if (view != NULL && lv_event_get_code(event) == LV_EVENT_CLICKED) {
        view->selected_hour = (uint8_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_target(event));
        forecast_state_snapshot_t state;
        forecast_state_get_snapshot(&state);
        ui_forecast_update(view, &state);
    }
}

void ui_forecast_init(ui_forecast_t *view, lv_obj_t *parent, lv_event_cb_t refresh_callback, void *refresh_user_data)
{
    lv_obj_t *card;
    if (view == NULL) return;
    memset(view, 0, sizeof(*view));

    card = ui_theme_card_create(parent, 16, 58, 688, 58, "天气预测");
    view->location = ui_theme_label_create(card, "位置未配置", 14, 32, &panel_zh_16, lv_color_hex(UI_TEXT_COLOR));
    view->risk = ui_theme_label_create(card, "未来三小时 --", 280, 32, &panel_zh_16, lv_color_hex(UI_MUTED_COLOR));
    lv_obj_t *button = lv_btn_create(card);
    lv_obj_set_pos(button, 552, 12);
    lv_obj_set_size(button, 122, 34);
    lv_obj_set_style_bg_color(button, lv_color_hex(UI_ACCENT_COLOR), 0);
    lv_obj_set_style_radius(button, 4, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_add_event_cb(button, refresh_callback, LV_EVENT_CLICKED, refresh_user_data);
    lv_obj_t *button_text = lv_label_create(button);
    lv_label_set_text(button_text, "刷新预报");
    lv_obj_set_style_text_font(button_text, &panel_zh_16, 0);
    lv_obj_set_style_text_color(button_text, lv_color_white(), 0);
    lv_obj_center(button_text);

    card = ui_theme_card_create(parent, 16, 130, 688, 222, "12 小时风雨趋势");
    ui_theme_label_create(card, "风速", 12, 40, &panel_zh_16, lv_color_hex(UI_MUTED_COLOR));
    ui_theme_label_create(card, "阵风", 12, 86, &panel_zh_16, lv_color_hex(UI_MUTED_COLOR));
    ui_theme_label_create(card, "降水", 12, 132, &panel_zh_16, lv_color_hex(UI_MUTED_COLOR));
    for (uint8_t index = 0; index < FORECAST_HOUR_COUNT; index++) {
        int x = 52 + index * 52;
        view->columns[index] = lv_obj_create(card);
        lv_obj_set_pos(view->columns[index], x, 34);
        lv_obj_set_size(view->columns[index], 48, 174);
        lv_obj_set_style_bg_opa(view->columns[index], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(view->columns[index], 0, 0);
        lv_obj_set_style_radius(view->columns[index], 3, 0);
        lv_obj_set_style_bg_color(view->columns[index], lv_color_hex(UI_SURFACE_MUTED), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(view->columns[index], LV_OPA_50, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(view->columns[index], 1, LV_STATE_CHECKED);
        lv_obj_set_style_border_color(view->columns[index], lv_color_hex(UI_ACCENT_COLOR), LV_STATE_CHECKED);
        lv_obj_add_flag(view->columns[index], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_user_data(view->columns[index], (void *)(uintptr_t)index);
        lv_obj_add_event_cb(view->columns[index], ui_forecast_hour_event, LV_EVENT_CLICKED, view);
        for (uint8_t metric = 0; metric < 3U; metric++) {
            lv_obj_t *bar = lv_bar_create(view->columns[index]);
            lv_obj_set_pos(bar, 7, 4 + metric * 44);
            lv_obj_set_size(bar, 34, 28);
            lv_bar_set_range(bar, 0, metric < 2U ? 20 : 100);
            lv_obj_set_style_bg_color(bar, lv_color_hex(UI_BG_COLOR), LV_PART_MAIN);
            lv_obj_set_style_radius(bar, 2, LV_PART_MAIN);
            lv_obj_set_style_radius(bar, 2, LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(bar, lv_color_hex(metric == 2U ? UI_ACCENT_COLOR : UI_ONLINE_COLOR), LV_PART_INDICATOR);
            if (metric == 0U) view->wind_bars[index] = bar;
            else if (metric == 1U) view->gust_bars[index] = bar;
            else view->rain_bars[index] = bar;
        }
        view->time_labels[index] = ui_theme_label_create(view->columns[index], "--", 0, 142, &lv_font_montserrat_12, lv_color_hex(UI_MUTED_COLOR));
        lv_obj_set_width(view->time_labels[index], 48);
        lv_obj_set_style_text_align(view->time_labels[index], LV_TEXT_ALIGN_CENTER, 0);
    }

    card = ui_theme_card_create(parent, 16, 366, 688, 82, "小时详情");
    view->detail = ui_theme_label_create(card, "等待预测服务数据", 14, 42, &panel_zh_20, lv_color_hex(UI_MUTED_COLOR));
    lv_label_set_long_mode(view->detail, LV_LABEL_LONG_DOT);
    lv_obj_set_width(view->detail, 660);
    view->status = ui_theme_label_create(parent, "数据源: Open-Meteo", 16, 456, &panel_zh_16, lv_color_hex(UI_MUTED_COLOR));
}

void ui_forecast_update(ui_forecast_t *view, const forecast_state_snapshot_t *state)
{
    if (view == NULL || state == NULL) return;
    if (!forecast_state_is_current(state)) {
        const char *message = !state->configured ? "预测未配置" :
                              state->status == FORECAST_STATUS_LOADING ? "正在刷新预报" :
                              (state->error[0] != '\0' ? state->error : "预测服务不可用");
        lv_label_set_text(view->location, state->location_name[0] ? state->location_name : "位置未配置");
        lv_label_set_text(view->risk, "未来三小时 不可用");
        lv_obj_set_style_text_color(view->risk, lv_color_hex(UI_MUTED_COLOR), 0);
        lv_label_set_text(view->detail, message);
        lv_label_set_text(view->status, "数据源: Open-Meteo | 请检查位置、Wi-Fi 与服务配置");
        for (uint8_t index = 0; index < FORECAST_HOUR_COUNT; index++) {
            lv_bar_set_value(view->wind_bars[index], 0, LV_ANIM_OFF);
            lv_bar_set_value(view->gust_bars[index], 0, LV_ANIM_OFF);
            lv_bar_set_value(view->rain_bars[index], 0, LV_ANIM_OFF);
            lv_label_set_text(view->time_labels[index], "--");
        }
        return;
    }

    lv_label_set_text(view->location, state->location_name[0] ? state->location_name : "未命名位置");
    lv_label_set_text_fmt(view->risk, "未来三小时 %s", ui_forecast_risk_text(state->next_three_hour_risk));
    lv_obj_set_style_text_color(view->risk, ui_forecast_risk_color(state->next_three_hour_risk), 0);
    lv_label_set_text_fmt(view->status, "数据源: %s | %s本地风雨校正", state->source[0] ? state->source : "Open-Meteo",
                          state->local_correction_applied ? "已应用" : "未应用");
    for (uint8_t index = 0; index < FORECAST_HOUR_COUNT; index++) {
        const forecast_hour_t *hour = &state->hours[index];
        const char *time = strlen(hour->time) >= 5U ? &hour->time[strlen(hour->time) - 5U] : hour->time;
        lv_bar_set_value(view->wind_bars[index], (int32_t)(hour->wind_speed_mps + 0.5f), LV_ANIM_OFF);
        lv_bar_set_value(view->gust_bars[index], (int32_t)(hour->wind_gust_mps + 0.5f), LV_ANIM_OFF);
        lv_bar_set_value(view->rain_bars[index], hour->precipitation_probability, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(view->wind_bars[index], ui_forecast_risk_color(hour->risk_level), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(view->gust_bars[index], ui_forecast_risk_color(hour->risk_level), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(view->columns[index], ui_forecast_risk_color(hour->risk_level), LV_STATE_CHECKED);
        if (index == view->selected_hour) lv_obj_add_state(view->columns[index], LV_STATE_CHECKED);
        else lv_obj_clear_state(view->columns[index], LV_STATE_CHECKED);
        lv_label_set_text(view->time_labels[index], time);
    }
    const forecast_hour_t *selected = &state->hours[view->selected_hour < FORECAST_HOUR_COUNT ? view->selected_hour : 0U];
    char detail[256];
    /* LVGL's compact formatter has floating-point support disabled to reduce
     * firmware size. Format forecast values with libc before updating LVGL. */
    (void)snprintf(detail, sizeof(detail),
                   "%s  风 %.1f m/s  阵风 %.1f m/s  降水 %u%% / %.1f mm  温度 %.1f C  湿度 %u%%  风向 %u deg",
                   selected->time, (double)selected->wind_speed_mps, (double)selected->wind_gust_mps,
                   selected->precipitation_probability, (double)selected->precipitation_mm,
                   (double)selected->temperature_c, selected->humidity_rh, selected->wind_direction_deg);
    lv_label_set_text(view->detail, detail);
}
