#include "ui.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "lvgl.h"

#include "ai_client.h"
#include "ai_state.h"
#include "forecast_client.h"
#include "forecast_state.h"
#include "lvgl_port.h"
#include "panel_protocol.h"
#include "panel_state.h"
#include "sensor_fusion.h"
#include "settings_manager.h"
#include "ui_air_quality.h"
#include "ui_forecast.h"
#include "ui_pad_gate.h"
#include "ui_settings.h"
#include "ui_theme.h"
#include "ui_wind.h"
#include "wifi_manager.h"

LV_FONT_DECLARE(panel_zh_20);
LV_FONT_DECLARE(panel_zh_16);

#define UI_PAGE_COUNT 7U
#define UI_NAV_WIDTH 96
#define UI_PAGE_WIDTH 704

static lv_obj_t *s_pages[UI_PAGE_COUNT];
static lv_obj_t *s_navigation[UI_PAGE_COUNT];
static uint8_t s_active_page;
static lv_obj_t *s_overview_link;
static lv_obj_t *s_overview_time;
static lv_obj_t *s_overview_date;
static lv_obj_t *s_overview_temp;
static lv_obj_t *s_overview_humidity;
static lv_obj_t *s_overview_wind;
static lv_obj_t *s_overview_rain;
static lv_obj_t *s_overview_air;
static lv_obj_t *s_overview_air_arc;
static lv_obj_t *s_overview_pad;
static lv_obj_t *s_overview_command;
static lv_obj_t *s_overview_temp_bar;
static lv_obj_t *s_overview_humidity_bar;
static lv_obj_t *s_weather_rain;
static lv_obj_t *s_weather_unavailable;
static lv_obj_t *s_control_command;
static lv_obj_t *s_system_link;
static lv_obj_t *s_system_counts;
static lv_obj_t *s_system_raw;
static lv_obj_t *s_system_touch;
static lv_obj_t *s_ai_network;
static lv_obj_t *s_ai_risk;
static lv_obj_t *s_ai_summary;
static lv_obj_t *s_ai_reasons;
static lv_obj_t *s_ai_recommendation;
static lv_obj_t *s_ai_forecast;
static lv_obj_t *s_ai_analyze_button;
static lv_obj_t *s_ai_adopt_button;
static lv_obj_t *s_confirm_box;
static lv_obj_t *s_control_buttons[4];
static panel_command_t s_confirm_command;
static ui_air_quality_t s_air_quality;
static ui_wind_t s_wind;
static ui_pad_gate_t s_pad_gate;
static ui_forecast_t s_forecast;
static ui_settings_t *s_settings;
static panel_state_snapshot_t s_last_panel_state;
static ai_state_snapshot_t s_last_ai_state;
static wifi_manager_snapshot_t s_last_wifi_state;
static settings_snapshot_t s_last_settings;
static forecast_state_snapshot_t s_last_forecast_state;
static bool s_have_panel_state;
static bool s_have_ai_state;
static bool s_have_wifi_state;
static bool s_have_settings;
static bool s_have_forecast_state;
static uint32_t s_last_system_age_second = UINT32_MAX;
static uint32_t s_last_clock_second = UINT32_MAX;
static bool s_touch_selftest_active;

static bool ui_snapshot_changed(const void *current, void *previous, size_t size, bool *initialized)
{
    if (!*initialized || memcmp(current, previous, size) != 0) {
        memcpy(previous, current, size);
        *initialized = true;
        return true;
    }
    return false;
}

static bool ui_panel_state_changed(const panel_state_snapshot_t *state)
{
    panel_state_snapshot_t normalized = *state;

    normalized.last_update_age_ms = 0U;
    return ui_snapshot_changed(&normalized, &s_last_panel_state, sizeof(normalized), &s_have_panel_state);
}

static const char *ui_command_name(panel_command_t command)
{
    switch (command) {
    case PANEL_COMMAND_PAD_OPEN: return "开启";
    case PANEL_COMMAND_PAD_CLOSE: return "关闭";
    case PANEL_COMMAND_PAD_STOP: return "紧急停止";
    case PANEL_COMMAND_QUERY_STATUS: return "刷新状态";
    default: return "暂无命令";
    }
}

static const char *ui_command_result_name(panel_command_result_t result)
{
    switch (result) {
    case PANEL_COMMAND_RESULT_SUCCESS: return "执行成功";
    case PANEL_COMMAND_RESULT_ERROR: return "执行失败";
    default: return "等待回执";
    }
}

static void ui_set_online_label(lv_obj_t *label, bool online)
{
    lv_label_set_text(label, online ? "CH32/RS485 在线" : "CH32/RS485 离线");
    lv_obj_set_style_text_color(label, lv_color_hex(online ? UI_ONLINE_COLOR : UI_DANGER_COLOR), 0);
}

static void ui_set_button_disabled(lv_obj_t *button, bool disabled)
{
    if (disabled) {
        lv_obj_add_state(button, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(button, LV_STATE_DISABLED);
    }
}

static void ui_set_active_page(uint8_t target)
{
    if (target >= UI_PAGE_COUNT || s_pages[target] == NULL) {
        return;
    }
    for (uint8_t index = 0; index < UI_PAGE_COUNT; index++) {
        if (index == target) {
            lv_obj_clear_flag(s_pages[index], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_state(s_navigation[index], LV_STATE_CHECKED);
        } else {
            lv_obj_add_flag(s_pages[index], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_state(s_navigation[index], LV_STATE_CHECKED);
        }
    }
    s_active_page = target;
}

static void ui_navigation_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        ui_set_active_page((uint8_t)(uintptr_t)lv_event_get_user_data(event));
    }
}

static lv_obj_t *ui_navigation_button(lv_obj_t *parent, uint8_t index, int y,
                                      const char *symbol, const char *label)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_t *icon = lv_label_create(button);
    lv_obj_t *text = lv_label_create(button);

    lv_obj_set_pos(button, 4, y);
    lv_obj_set_size(button, 88, 56);
    lv_obj_set_style_radius(button, 6, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(UI_SURFACE_MUTED), LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(button, LV_OPA_40, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_border_width(button, 3, LV_STATE_CHECKED);
    lv_obj_set_style_border_side(button, LV_BORDER_SIDE_LEFT, LV_STATE_CHECKED);
    lv_obj_set_style_border_color(button, lv_color_hex(UI_ACCENT_COLOR), LV_STATE_CHECKED);
    lv_obj_set_style_anim_time(button, 150, 0);
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(button, ui_navigation_event, LV_EVENT_CLICKED, (void *)(uintptr_t)index);
    lv_label_set_text(icon, symbol);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(UI_TEXT_COLOR), 0);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 6);
    lv_label_set_text(text, label);
    lv_obj_set_style_text_font(text, &panel_zh_16, 0);
    lv_obj_set_style_text_color(text, lv_color_hex(UI_MUTED_COLOR), 0);
    lv_obj_set_width(text, 82);
    lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(text, LV_ALIGN_BOTTOM_MID, 0, -4);
    return button;
}

static lv_obj_t *ui_page_create(void)
{
    lv_obj_t *page = lv_obj_create(lv_scr_act());

    lv_obj_set_pos(page, UI_NAV_WIDTH, 0);
    lv_obj_set_size(page, UI_PAGE_WIDTH, 480);
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    return page;
}

static void ui_page_header(lv_obj_t *page, const char *title, lv_obj_t **status)
{
    lv_obj_t *line = lv_obj_create(page);
    ui_theme_label_create(page, title, 16, 12, &panel_zh_20, lv_color_hex(UI_TEXT_COLOR));
    lv_obj_set_pos(line, 16, 42);
    lv_obj_set_size(line, 688, 1);
    lv_obj_set_style_bg_color(line, lv_color_hex(UI_BORDER_COLOR), 0);
    lv_obj_set_style_border_width(line, 0, 0);
    if (status != NULL) {
        *status = ui_theme_label_create(page, "等待数据", 518, 14, &panel_zh_16, lv_color_hex(UI_MUTED_COLOR));
        lv_obj_set_width(*status, 186);
        lv_obj_set_style_text_align(*status, LV_TEXT_ALIGN_RIGHT, 0);
    }
}

static lv_obj_t *ui_button_create(lv_obj_t *parent, int x, int y, int width, int height,
                                  const char *text, lv_color_t color, lv_event_cb_t callback, void *user_data)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_t *label = lv_label_create(button);

    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_radius(button, 6, 0);
    lv_obj_set_style_bg_color(button, color, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(UI_SURFACE_MUTED), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, color, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_40, LV_STATE_DISABLED);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &panel_zh_20, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
    return button;
}

static void ui_confirm_event(lv_event_t *event)
{
    lv_obj_t *box = lv_event_get_current_target(event);
    if (lv_msgbox_get_active_btn(box) == 0U) {
        panel_state_request_command(s_confirm_command);
    }
    lv_msgbox_close(box);
    s_confirm_box = NULL;
}

static void ui_show_confirm(panel_command_t command)
{
    static const char *buttons[] = { "确认", "取消", "" };
    const char *message = command == PANEL_COMMAND_PAD_OPEN ? "确认开启停机坪？" : "确认关闭停机坪？";

    if (s_confirm_box != NULL) {
        return;
    }
    s_confirm_command = command;
    s_confirm_box = lv_msgbox_create(NULL, "停机坪控制", message, buttons, true);
    lv_obj_set_style_text_font(s_confirm_box, &panel_zh_20, 0);
    lv_obj_set_style_bg_color(s_confirm_box, lv_color_hex(UI_SURFACE_COLOR), 0);
    lv_obj_set_style_text_color(s_confirm_box, lv_color_hex(UI_TEXT_COLOR), 0);
    lv_obj_set_width(s_confirm_box, 360);
    lv_obj_center(s_confirm_box);
    lv_obj_add_event_cb(s_confirm_box, ui_confirm_event, LV_EVENT_VALUE_CHANGED, NULL);
}

static void ui_control_event(lv_event_t *event)
{
    panel_command_t command = (panel_command_t)(uintptr_t)lv_event_get_user_data(event);
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    if (command == PANEL_COMMAND_PAD_OPEN || command == PANEL_COMMAND_PAD_CLOSE) {
        ui_show_confirm(command);
    } else {
        panel_state_request_command(command);
    }
}

static void ui_touch_selftest_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        s_touch_selftest_active = true;
        lv_label_set_text(s_system_touch, "触摸自检已启动，请点按任意控件");
    }
}

static void ui_ai_analyze_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        ai_client_request_analysis();
    }
}

static void ui_forecast_refresh_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        forecast_client_request_refresh();
    }
}

static void ui_ai_adopt_event(lv_event_t *event)
{
    ai_state_snapshot_t ai;
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    ai_state_get_snapshot(&ai);
    if (ai.request_status == AI_REQUEST_READY && ai.fusion.data_valid &&
        ai.recommended_command != PANEL_COMMAND_NONE && ai.requires_confirmation) {
        ui_show_confirm(ai.recommended_command);
    }
}

static void ui_overview_shortcut_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        ui_set_active_page((uint8_t)(uintptr_t)lv_event_get_user_data(event));
    }
}

static void ui_create_overview(lv_obj_t *page)
{
    lv_obj_t *card;
    lv_obj_t *label;

    ui_page_header(page, "运行总览", &s_overview_link);
    s_overview_time = ui_theme_label_create(page, "--:--:--", 250, 48, &lv_font_montserrat_24, lv_color_hex(UI_TEXT_COLOR));
    lv_obj_set_width(s_overview_time, 220);
    lv_obj_set_style_text_align(s_overview_time, LV_TEXT_ALIGN_CENTER, 0);
    s_overview_date = ui_theme_label_create(page, "时间未同步", 250, 78, &panel_zh_20, lv_color_hex(UI_MUTED_COLOR));
    lv_obj_set_width(s_overview_date, 220);
    lv_obj_set_style_text_align(s_overview_date, LV_TEXT_ALIGN_CENTER, 0);

    card = ui_theme_card_create(page, 16, 108, 220, 154, "本地气象");
    s_overview_temp = ui_theme_value_label_create(card, 14, 42, 126);
    s_overview_humidity = ui_theme_value_label_create(card, 14, 80, 126);
    s_overview_wind = ui_theme_value_label_create(card, 14, 116, 194);

    card = ui_theme_card_create(page, 252, 108, 216, 154, "空气质量");
    s_overview_air_arc = lv_arc_create(card);
    lv_obj_set_pos(s_overview_air_arc, 48, 28);
    lv_obj_set_size(s_overview_air_arc, 120, 120);
    lv_arc_set_range(s_overview_air_arc, 0, 150);
    lv_arc_set_rotation(s_overview_air_arc, 135);
    lv_arc_set_bg_angles(s_overview_air_arc, 0, 270);
    lv_obj_set_style_arc_width(s_overview_air_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_overview_air_arc, lv_color_hex(UI_SURFACE_MUTED), LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_overview_air_arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_overview_air_arc, lv_color_hex(UI_ACCENT_COLOR), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_overview_air_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    s_overview_air = ui_theme_label_create(card, "-- ug/m3", 32, 79, &panel_zh_20, lv_color_hex(UI_TEXT_COLOR));
    lv_obj_set_width(s_overview_air, 152);
    lv_obj_set_style_text_align(s_overview_air, LV_TEXT_ALIGN_CENTER, 0);

    card = ui_theme_card_create(page, 484, 108, 220, 154, "降雨与环境");
    s_overview_rain = ui_theme_value_label_create(card, 14, 42, 122);
    lv_obj_set_style_text_font(s_overview_rain, &panel_zh_20, 0);
    label = ui_theme_label_create(card, "温度", 144, 42, &panel_zh_20, lv_color_hex(UI_MUTED_COLOR));
    (void)label;
    s_overview_temp_bar = lv_bar_create(card);
    lv_obj_set_pos(s_overview_temp_bar, 154, 66);
    lv_obj_set_size(s_overview_temp_bar, 18, 68);
    lv_bar_set_range(s_overview_temp_bar, -20, 50);
    lv_obj_set_style_bg_color(s_overview_temp_bar, lv_color_hex(UI_SURFACE_MUTED), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_overview_temp_bar, lv_color_hex(UI_DANGER_COLOR), LV_PART_INDICATOR);
    label = ui_theme_label_create(card, "湿度", 180, 42, &panel_zh_20, lv_color_hex(UI_MUTED_COLOR));
    (void)label;
    s_overview_humidity_bar = lv_bar_create(card);
    lv_obj_set_pos(s_overview_humidity_bar, 190, 66);
    lv_obj_set_size(s_overview_humidity_bar, 18, 68);
    lv_bar_set_range(s_overview_humidity_bar, 0, 100);
    lv_obj_set_style_bg_color(s_overview_humidity_bar, lv_color_hex(UI_SURFACE_MUTED), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_overview_humidity_bar, lv_color_hex(UI_ACCENT_COLOR), LV_PART_INDICATOR);

    card = ui_theme_card_create(page, 16, 278, 336, 104, "停机坪通道");
    s_overview_pad = ui_theme_value_label_create(card, 14, 44, 308);
    card = ui_theme_card_create(page, 368, 278, 336, 104, "最近命令");
    s_overview_command = ui_theme_value_label_create(card, 14, 44, 308);
    (void)ui_button_create(page, 16, 400, 216, 48, "气象实测", lv_color_hex(UI_SURFACE_MUTED), ui_overview_shortcut_event, (void *)1U);
    (void)ui_button_create(page, 252, 400, 216, 48, "停机坪控制", lv_color_hex(UI_ACCENT_COLOR), ui_overview_shortcut_event, (void *)2U);
    (void)ui_button_create(page, 488, 400, 216, 48, "AI 研判", lv_color_hex(UI_SURFACE_MUTED), ui_overview_shortcut_event, (void *)4U);
}

static void ui_create_forecast(lv_obj_t *page)
{
    ui_page_header(page, "天气预测", NULL);
    ui_forecast_init(&s_forecast, page, ui_forecast_refresh_event, NULL);
}

static void ui_create_weather(lv_obj_t *page)
{
    lv_obj_t *card;
    ui_page_header(page, "气象实测", NULL);
    ui_wind_init(&s_wind, page, 16, 58, 328, 270);
    ui_air_quality_init(&s_air_quality, page, 360, 58, 344, 270);
    card = ui_theme_card_create(page, 16, 344, 688, 104, "降雨与未接入指标");
    s_weather_rain = ui_theme_value_label_create(card, 14, 42, 404);
    s_weather_unavailable = ui_theme_value_label_create(card, 430, 42, 242);
}

static void ui_create_control(lv_obj_t *page)
{
    lv_obj_t *card;
    ui_page_header(page, "停机坪控制", NULL);
    ui_pad_gate_init(&s_pad_gate, page, 16, 58, 388, 282);
    card = ui_theme_card_create(page, 420, 58, 284, 124, "命令回执");
    s_control_command = ui_theme_value_label_create(card, 14, 42, 256);
    s_control_buttons[0] = ui_button_create(page, 420, 202, 134, 60, "开启", lv_color_hex(UI_ACCENT_COLOR), ui_control_event, (void *)PANEL_COMMAND_PAD_OPEN);
    s_control_buttons[1] = ui_button_create(page, 570, 202, 134, 60, "关闭", lv_color_hex(UI_WARNING_COLOR), ui_control_event, (void *)PANEL_COMMAND_PAD_CLOSE);
    s_control_buttons[2] = ui_button_create(page, 420, 278, 134, 60, "紧急停止", lv_color_hex(UI_DANGER_COLOR), ui_control_event, (void *)PANEL_COMMAND_PAD_STOP);
    s_control_buttons[3] = ui_button_create(page, 570, 278, 134, 60, "刷新状态", lv_color_hex(UI_SURFACE_MUTED), ui_control_event, (void *)PANEL_COMMAND_QUERY_STATUS);
    card = ui_theme_card_create(page, 420, 354, 284, 94, "安全提示");
    ui_theme_label_create(card, "开启和关闭均需二次确认\n状态仅以 CH32 回执为准", 14, 40, &panel_zh_20, lv_color_hex(UI_MUTED_COLOR));
}

static void ui_create_system(lv_obj_t *page)
{
    lv_obj_t *card;
    ui_page_header(page, "系统诊断", NULL);
    card = ui_theme_card_create(page, 16, 58, 216, 214, "RS485 链路");
    s_system_link = ui_theme_value_label_create(card, 14, 44, 188);
    card = ui_theme_card_create(page, 252, 58, 216, 214, "收发统计");
    s_system_counts = ui_theme_value_label_create(card, 14, 44, 188);
    card = ui_theme_card_create(page, 488, 58, 216, 214, "原始状态");
    s_system_raw = ui_theme_value_label_create(card, 14, 44, 188);
    card = ui_theme_card_create(page, 16, 292, 688, 156, "触摸自检");
    s_system_touch = ui_theme_value_label_create(card, 14, 44, 430);
    (void)ui_button_create(card, 488, 48, 176, 52, "开始自检", lv_color_hex(UI_ACCENT_COLOR), ui_touch_selftest_event, NULL);
}

static void ui_create_ai(lv_obj_t *page)
{
    lv_obj_t *card;
    ui_page_header(page, "AI 研判", &s_ai_network);
    card = ui_theme_card_create(page, 16, 58, 198, 118, "融合风险");
    s_ai_risk = ui_theme_value_label_create(card, 14, 42, 170);
    lv_obj_set_style_text_font(s_ai_risk, &panel_zh_20, 0);
    s_ai_forecast = ui_theme_label_create(card, "未来风险 --", 14, 78, &panel_zh_16, lv_color_hex(UI_MUTED_COLOR));
    card = ui_theme_card_create(page, 232, 58, 472, 118, "模型摘要");
    s_ai_summary = ui_theme_value_label_create(card, 14, 44, 444);
    lv_label_set_long_mode(s_ai_summary, LV_LABEL_LONG_WRAP);
    card = ui_theme_card_create(page, 16, 194, 430, 166, "研判原因");
    s_ai_reasons = ui_theme_value_label_create(card, 14, 40, 402);
    lv_label_set_long_mode(s_ai_reasons, LV_LABEL_LONG_WRAP);
    card = ui_theme_card_create(page, 464, 194, 240, 166, "操作建议");
    s_ai_recommendation = ui_theme_value_label_create(card, 14, 40, 212);
    lv_label_set_long_mode(s_ai_recommendation, LV_LABEL_LONG_WRAP);
    s_ai_analyze_button = ui_button_create(page, 16, 382, 210, 58, "开始 AI 分析", lv_color_hex(UI_ACCENT_COLOR), ui_ai_analyze_event, NULL);
    s_ai_adopt_button = ui_button_create(page, 242, 382, 210, 58, "采用建议", lv_color_hex(UI_WARNING_COLOR), ui_ai_adopt_event, NULL);
}

static void ui_update_clock(void)
{
    time_t now = time(NULL);
    struct tm calendar;
    char date[64];

    if (now < 1704067200) {
        if (s_last_clock_second != 0U) {
            lv_label_set_text(s_overview_time, "--:--:--");
            lv_label_set_text(s_overview_date, "时间未同步");
            s_last_clock_second = 0U;
        }
        return;
    }
    if ((uint32_t)now == s_last_clock_second) {
        return;
    }
    localtime_r(&now, &calendar);
    lv_label_set_text_fmt(s_overview_time, "%02d:%02d:%02d", calendar.tm_hour, calendar.tm_min, calendar.tm_sec);
    (void)strftime(date, sizeof(date), "%Y-%m-%d", &calendar);
    lv_label_set_text(s_overview_date, date);
    s_last_clock_second = (uint32_t)now;
}

static void ui_refresh(lv_timer_t *timer)
{
    panel_state_snapshot_t state;
    ai_state_snapshot_t ai;
    forecast_state_snapshot_t forecast;
    wifi_manager_snapshot_t wifi;
    settings_snapshot_t settings;
    bool status_current;
    bool command_pending;
    bool panel_changed;
    bool ai_changed;
    bool wifi_changed;
    bool settings_changed;
    uint32_t age_second;
    char temperature[20];
    char humidity[20];
    char pm25[20];
    char wind_speed[20];
    char wind_direction[20];
    char rain_value[20];

    (void)timer;
    panel_state_get_snapshot(&state);
    ai_state_get_snapshot(&ai);
    wifi_manager_get_snapshot(&wifi);
    settings_manager_get_snapshot(&settings);
    forecast_state_get_snapshot(&forecast);
    panel_changed = ui_panel_state_changed(&state);
    ai_changed = ui_snapshot_changed(&ai, &s_last_ai_state, sizeof(ai), &s_have_ai_state);
    wifi_changed = ui_snapshot_changed(&wifi, &s_last_wifi_state, sizeof(wifi), &s_have_wifi_state);
    settings_changed = ui_snapshot_changed(&settings, &s_last_settings, sizeof(settings), &s_have_settings);
    bool forecast_changed = ui_snapshot_changed(&forecast, &s_last_forecast_state, sizeof(forecast), &s_have_forecast_state);
    status_current = state.h743_online && state.status_valid;
    command_pending = ui_pad_gate_is_pending(&state);
    ui_update_clock();

    if (forecast_changed) {
        ui_forecast_update(&s_forecast, &forecast);
        if (forecast_state_is_current(&forecast)) {
            const char *risk = forecast.next_three_hour_risk == FORECAST_RISK_DANGER ? "高风险" :
                               forecast.next_three_hour_risk == FORECAST_RISK_CAUTION ? "关注" : "正常";
            lv_label_set_text_fmt(s_ai_forecast, "未来风险 %s", risk);
            lv_obj_set_style_text_color(s_ai_forecast,
                                        lv_color_hex(forecast.next_three_hour_risk == FORECAST_RISK_DANGER ? UI_DANGER_COLOR :
                                                     forecast.next_three_hour_risk == FORECAST_RISK_CAUTION ? UI_WARNING_COLOR : UI_ONLINE_COLOR), 0);
        } else {
            lv_label_set_text(s_ai_forecast, "未来风险 不可用");
            lv_obj_set_style_text_color(s_ai_forecast, lv_color_hex(UI_MUTED_COLOR), 0);
        }
    }

    if (panel_changed) {
        ui_set_online_label(s_overview_link, state.h743_online);
        if (state.air_online) {
            ui_theme_format_decimal(temperature, sizeof(temperature), state.temperature_c, 1U);
            ui_theme_format_decimal(humidity, sizeof(humidity), state.humidity_rh, 0U);
            ui_theme_format_decimal(pm25, sizeof(pm25), state.pm25_ugm3, 0U);
            lv_label_set_text_fmt(s_overview_temp, "%s C", temperature);
            lv_label_set_text_fmt(s_overview_humidity, "%s %%", humidity);
            lv_bar_set_value(s_overview_temp_bar, (int32_t)state.temperature_c, LV_ANIM_OFF);
            lv_bar_set_value(s_overview_humidity_bar, (int32_t)state.humidity_rh, LV_ANIM_OFF);
            lv_arc_set_value(s_overview_air_arc, (int32_t)(state.pm25_ugm3 * 100.0f / 35.0f));
            lv_label_set_text_fmt(s_overview_air, "%s ug/m3", pm25);
        } else {
            lv_label_set_text(s_overview_temp, "温湿度离线");
            lv_label_set_text(s_overview_humidity, "--");
            lv_bar_set_value(s_overview_temp_bar, -20, LV_ANIM_OFF);
            lv_bar_set_value(s_overview_humidity_bar, 0, LV_ANIM_OFF);
            lv_arc_set_value(s_overview_air_arc, 0);
            lv_label_set_text(s_overview_air, "传感器离线");
        }
        if (state.wind_online) {
            ui_theme_format_decimal(wind_speed, sizeof(wind_speed), state.wind_speed_mps, 1U);
            ui_theme_format_decimal(wind_direction, sizeof(wind_direction), state.wind_direction_deg, 0U);
            lv_label_set_text_fmt(s_overview_wind, "风  %s m/s   %s deg", wind_speed, wind_direction);
        } else {
            lv_label_set_text(s_overview_wind, "风场传感器离线");
        }
        lv_label_set_text(s_overview_rain, state.rain_online ? (state.rain_detected ? "检测到降雨" : "当前无降雨") : "降雨传感器离线");
        if (status_current) {
            lv_label_set_text_fmt(s_overview_pad, "左通道 %s\n右通道 %s", state.relay1_on ? "开启" : "关闭", state.relay2_on ? "开启" : "关闭");
        } else {
            lv_label_set_text(s_overview_pad, "等待 CH32 状态帧");
        }
        lv_label_set_text_fmt(s_overview_command, "%s\n%s", ui_command_name(state.last_command),
                              command_pending ? "命令发送中" : ui_command_result_name(state.last_command_result));

        ui_wind_update(&s_wind, &state);
        ui_air_quality_update(&s_air_quality, &state);
        if (state.rain_online) {
            ui_theme_format_decimal(rain_value, sizeof(rain_value), state.rain_value, 0U);
            ui_theme_format_decimal(humidity, sizeof(humidity), state.humidity_rh, 0U);
            lv_label_set_text_fmt(s_weather_rain, "降雨  %s\n雨滴 %s    湿度 %s %%",
                                  state.rain_detected ? "检测到" : "未检测到", rain_value, humidity);
        } else {
            lv_label_set_text(s_weather_rain, "降雨传感器离线");
        }
        lv_label_set_text(s_weather_unavailable, "气压  未接入\n能见度  未接入");

        ui_pad_gate_update(&s_pad_gate, &state);
        lv_label_set_text_fmt(s_control_command, "最后命令  %s\n状态  %s\n回执序号  %lu", ui_command_name(state.last_command),
                              command_pending ? "命令发送中" : (state.command_timed_out ? "链路超时" : ui_command_result_name(state.last_command_result)),
                              (unsigned long)state.command_completion_id);
        ui_set_button_disabled(s_control_buttons[0], command_pending || !state.h743_online);
        ui_set_button_disabled(s_control_buttons[1], command_pending || !state.h743_online);
        ui_set_button_disabled(s_control_buttons[3], command_pending || !state.h743_online);
    }

    age_second = state.last_update_age_ms / 1000U;
    if (panel_changed || age_second != s_last_system_age_second) {
        s_last_system_age_second = age_second;
        lv_label_set_text_fmt(s_system_link, "状态  %s\n更新  %lu ms\n序号  %u", state.h743_online ? "在线" : "离线",
                              (unsigned long)state.last_update_age_ms, state.last_sequence);
        lv_label_set_text_fmt(s_system_counts, "RX  %lu   TX  %lu\nCRC %lu\n格式 %lu   超时 %lu", (unsigned long)state.rx_frames,
                              (unsigned long)state.tx_frames, (unsigned long)state.crc_errors,
                              (unsigned long)state.format_errors, (unsigned long)state.timeout_errors);
        lv_label_set_text_fmt(s_system_raw, "帧 %u / %u\n风速 %u  风向 %u\n雨滴 %u\nFW 1.0 / P1", state.last_sequence,
                              state.last_operation, state.wind_speed_raw, state.wind_direction_raw, state.rain_raw);
    }

    if (s_system_touch != NULL) {
        lvgl_port_touch_snapshot_t touch;
        lvgl_port_get_touch_snapshot(&touch);
        lv_label_set_text_fmt(s_system_touch, "%s\nGT911 %s  X:%u  Y:%u  样本 #%lu",
                              s_touch_selftest_active ? "触摸自检中" : "等待自检", touch.pressed ? "按下" : "就绪",
                              touch.x, touch.y, (unsigned long)touch.sequence);
    }

    if (wifi_changed || settings_changed) {
        if (!wifi.configured) {
            lv_label_set_text(s_ai_network, "Wi-Fi 未配置");
            lv_obj_set_style_text_color(s_ai_network, lv_color_hex(UI_WARNING_COLOR), 0);
        } else if (!wifi.connected) {
            lv_label_set_text(s_ai_network, "Wi-Fi 连接中");
            lv_obj_set_style_text_color(s_ai_network, lv_color_hex(UI_WARNING_COLOR), 0);
        } else if (settings.edge_api_url[0] == '\0' || settings.edge_key[0] == '\0') {
            lv_label_set_text(s_ai_network, "Edge 未配置");
            lv_obj_set_style_text_color(s_ai_network, lv_color_hex(UI_WARNING_COLOR), 0);
        } else {
            lv_label_set_text_fmt(s_ai_network, "Wi-Fi %d dBm", wifi.rssi);
            lv_obj_set_style_text_color(s_ai_network, lv_color_hex(UI_ONLINE_COLOR), 0);
        }
    }

    if (ai_changed) {
        switch (ai.fusion.level) {
        case SENSOR_RISK_NORMAL:
            lv_label_set_text(s_ai_risk, "风险：正常");
            lv_obj_set_style_text_color(s_ai_risk, lv_color_hex(UI_ONLINE_COLOR), 0);
            break;
        case SENSOR_RISK_CAUTION:
            lv_label_set_text(s_ai_risk, "风险：关注");
            lv_obj_set_style_text_color(s_ai_risk, lv_color_hex(UI_WARNING_COLOR), 0);
            break;
        case SENSOR_RISK_DANGER:
            lv_label_set_text(s_ai_risk, "风险：高风险");
            lv_obj_set_style_text_color(s_ai_risk, lv_color_hex(UI_DANGER_COLOR), 0);
            break;
        default:
            lv_label_set_text(s_ai_risk, "风险：未知");
            lv_obj_set_style_text_color(s_ai_risk, lv_color_hex(UI_MUTED_COLOR), 0);
            break;
        }
        if (ai.request_status == AI_REQUEST_READY) {
            lv_label_set_text(s_ai_summary, ai.summary[0] != '\0' ? ai.summary : "未收到模型摘要");
            lv_label_set_text_fmt(s_ai_reasons, "1. %s\n2. %s\n3. %s", ai.reasons[0][0] != '\0' ? ai.reasons[0] : "-",
                                  ai.reasons[1][0] != '\0' ? ai.reasons[1] : "-", ai.reasons[2][0] != '\0' ? ai.reasons[2] : "-");
            lv_label_set_text_fmt(s_ai_recommendation, "建议：%s\n需人工确认", ui_command_name(ai.recommended_command));
        } else if (ai.request_status == AI_REQUEST_IN_PROGRESS) {
            lv_label_set_text(s_ai_summary, "云端模型正在分析真实传感器数据");
            lv_label_set_text(s_ai_reasons, "分析期间不会自动发送继电器命令");
            lv_label_set_text(s_ai_recommendation, "状态：分析中");
        } else if (ai.request_status == AI_REQUEST_ERROR) {
            lv_label_set_text(s_ai_summary, "AI 服务暂不可用");
            lv_label_set_text(s_ai_reasons, ai.error[0] != '\0' ? ai.error : "请检查网络与服务配置");
            lv_label_set_text(s_ai_recommendation, "状态：不执行控制");
        } else {
            lv_label_set_text(s_ai_summary, "网络就绪后可请求 AI 分析");
            lv_label_set_text(s_ai_reasons, "仅使用真实 RS485 传感器数据");
            lv_label_set_text(s_ai_recommendation, "状态：等待分析");
        }
    }

    if (panel_changed || ai_changed || wifi_changed || settings_changed) {
        ui_set_button_disabled(s_ai_analyze_button, !wifi.connected || !state.h743_online || !state.status_valid);
        ui_set_button_disabled(s_ai_adopt_button, ai.request_status != AI_REQUEST_READY || !ai.fusion.data_valid ||
                               ai.recommended_command == PANEL_COMMAND_NONE || !ai.requires_confirmation ||
                               state.pending_command != PANEL_COMMAND_NONE);
    }
    if (s_settings != NULL && (panel_changed || ai_changed || wifi_changed || settings_changed || ui_settings_needs_refresh(s_settings))) {
        ui_settings_update(s_settings, &state, &wifi, &ai);
    }
}

void panel_ui_init(void)
{
    lv_obj_t *nav;

    (void)setenv("TZ", "CST-8", 1);
    tzset();
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(UI_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

    for (uint8_t index = 0; index < UI_PAGE_COUNT; index++) {
        s_pages[index] = ui_page_create();
    }
    ui_create_overview(s_pages[0]);
    ui_create_weather(s_pages[1]);
    ui_create_control(s_pages[2]);
    ui_create_forecast(s_pages[3]);
    ui_create_ai(s_pages[4]);
    ui_create_system(s_pages[5]);
    s_settings = ui_settings_create(s_pages[6]);

    nav = lv_obj_create(lv_scr_act());
    lv_obj_set_pos(nav, 0, 0);
    lv_obj_set_size(nav, UI_NAV_WIDTH, 480);
    lv_obj_set_style_bg_color(nav, lv_color_hex(0x1B1D1E), 0);
    lv_obj_set_style_border_width(nav, 1, 0);
    lv_obj_set_style_border_side(nav, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_color(nav, lv_color_hex(UI_BORDER_COLOR), 0);
    lv_obj_set_style_pad_all(nav, 0, 0);
    lv_obj_clear_flag(nav, LV_OBJ_FLAG_SCROLLABLE);
    ui_theme_label_create(nav, "AIoT", 28, 20, &lv_font_montserrat_16, lv_color_hex(UI_ACCENT_COLOR));
    ui_theme_label_create(nav, "PANEL", 27, 43, &lv_font_montserrat_12, lv_color_hex(UI_MUTED_COLOR));
    s_navigation[0] = ui_navigation_button(nav, 0, 72, LV_SYMBOL_HOME, "概览");
    s_navigation[1] = ui_navigation_button(nav, 1, 128, LV_SYMBOL_EYE_OPEN, "气象");
    s_navigation[2] = ui_navigation_button(nav, 2, 184, LV_SYMBOL_SETTINGS, "机坪");
    s_navigation[3] = ui_navigation_button(nav, 3, 240, LV_SYMBOL_REFRESH, "预测");
    s_navigation[4] = ui_navigation_button(nav, 4, 296, LV_SYMBOL_WIFI, "研判");
    s_navigation[5] = ui_navigation_button(nav, 5, 352, LV_SYMBOL_LIST, "系统");
    s_navigation[6] = ui_navigation_button(nav, 6, 408, LV_SYMBOL_SETTINGS, "设置");
    ui_set_active_page(0);

    lv_timer_create(ui_refresh, 500, NULL);
    ui_refresh(NULL);
}
