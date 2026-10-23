#include "ui_settings.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"

#include "settings_manager.h"
#include "forecast_client.h"
#include "ui_theme.h"

LV_FONT_DECLARE(panel_zh_20);

typedef enum {
    SETTINGS_FIELD_UNLOCK = 0,
    SETTINGS_FIELD_SSID,
    SETTINGS_FIELD_WIFI_PASSWORD,
    SETTINGS_FIELD_EDGE_URL,
    SETTINGS_FIELD_MODEL,
    SETTINGS_FIELD_DEVICE_ID,
    SETTINGS_FIELD_EDGE_KEY,
    SETTINGS_FIELD_INTERVAL,
    SETTINGS_FIELD_ADMIN_PIN,
    SETTINGS_FIELD_FORECAST_URL,
    SETTINGS_FIELD_FORECAST_KEY,
    SETTINGS_FIELD_FORECAST_LOCATION,
    SETTINGS_FIELD_FORECAST_LATITUDE,
    SETTINGS_FIELD_FORECAST_LONGITUDE,
    SETTINGS_FIELD_FORECAST_TIMEZONE,
    SETTINGS_FIELD_FORECAST_INTERVAL,
} settings_field_t;

struct ui_settings {
    lv_obj_t *link_nodes[4];
    lv_obj_t *link_status;
    lv_obj_t *network_info;
    lv_obj_t *edge_info;
    lv_obj_t *security_info;
    lv_obj_t *field_buttons[7];
    lv_obj_t *unlock_button;
    lv_obj_t *save_button;
    lv_obj_t *restore_button;
    lv_obj_t *lock_button;
    lv_obj_t *editor_layer;
    lv_obj_t *editor_title;
    lv_obj_t *editor_input;
    lv_obj_t *editor_keyboard;
    lv_obj_t *editor_hint;
    lv_obj_t *restore_confirm;
    settings_field_t editing_field;
    settings_field_t deferred_field;
    settings_snapshot_t draft;
    bool unlocked;
    bool editor_upper;
    bool refresh_pending;
    char status[80];
};

static const char *s_number_keys[] = {"1", "2", "3", "BK", "\n", "4", "5", "6", "CLR", "\n", "7", "8", "9", "OK", "\n", "-", "0", ".", "CANCEL", ""};
static const char *s_ascii_lower[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "BK", "\n", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "-", "\n", "a", "s", "d", "f", "g", "h", "j", "k", "l", "_", "@", "\n", "CASE", "z", "x", "c", "v", "b", "n", "m", ".", "/", "CLR", "\n", "SPACE", ":", "OK", "CANCEL", ""};
static const char *s_ascii_upper[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "BK", "\n", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "-", "\n", "A", "S", "D", "F", "G", "H", "J", "K", "L", "_", "@", "\n", "CASE", "Z", "X", "C", "V", "B", "N", "M", ".", "/", "CLR", "\n", "SPACE", ":", "OK", "CANCEL", ""};

static void ui_settings_show_editor(ui_settings_t *view, settings_field_t field);

static settings_field_t ui_settings_next_forecast_field(settings_field_t field)
{
    switch (field) {
    case SETTINGS_FIELD_FORECAST_URL: return SETTINGS_FIELD_FORECAST_KEY;
    case SETTINGS_FIELD_FORECAST_KEY: return SETTINGS_FIELD_FORECAST_LOCATION;
    case SETTINGS_FIELD_FORECAST_LOCATION: return SETTINGS_FIELD_FORECAST_LATITUDE;
    case SETTINGS_FIELD_FORECAST_LATITUDE: return SETTINGS_FIELD_FORECAST_LONGITUDE;
    case SETTINGS_FIELD_FORECAST_LONGITUDE: return SETTINGS_FIELD_FORECAST_TIMEZONE;
    case SETTINGS_FIELD_FORECAST_TIMEZONE: return SETTINGS_FIELD_FORECAST_INTERVAL;
    default: return SETTINGS_FIELD_UNLOCK;
    }
}

static const char *ui_settings_field_name(settings_field_t field)
{
    switch (field) {
    case SETTINGS_FIELD_SSID:
        return "Wi-Fi 名称";
    case SETTINGS_FIELD_WIFI_PASSWORD:
        return "Wi-Fi 密码";
    case SETTINGS_FIELD_EDGE_URL:
        return "AI API 地址";
    case SETTINGS_FIELD_MODEL:
        return "模型名称";
    case SETTINGS_FIELD_DEVICE_ID:
        return "设备 ID";
    case SETTINGS_FIELD_EDGE_KEY:
        return "AI API Key";
    case SETTINGS_FIELD_INTERVAL:
        return "AI 周期";
    case SETTINGS_FIELD_ADMIN_PIN:
        return "管理员 PIN";
    case SETTINGS_FIELD_FORECAST_URL:
        return "预报服务地址";
    case SETTINGS_FIELD_FORECAST_KEY:
        return "预报服务 Key";
    case SETTINGS_FIELD_FORECAST_LOCATION:
        return "地点名称";
    case SETTINGS_FIELD_FORECAST_LATITUDE:
        return "纬度";
    case SETTINGS_FIELD_FORECAST_LONGITUDE:
        return "经度";
    case SETTINGS_FIELD_FORECAST_TIMEZONE:
        return "时区";
    case SETTINGS_FIELD_FORECAST_INTERVAL:
        return "预报刷新周期";
    default:
        return "管理员解锁";
    }
}

static void ui_settings_set_status(ui_settings_t *view, const char *status)
{
    strlcpy(view->status, status, sizeof(view->status));
    view->refresh_pending = true;
}

static void ui_settings_set_node(lv_obj_t *node, bool online, bool pending)
{
    lv_color_t color = pending ? lv_color_hex(UI_WARNING_COLOR) :
                       (online ? lv_color_hex(UI_ONLINE_COLOR) : lv_color_hex(UI_DANGER_COLOR));
    lv_obj_set_style_bg_color(node, color, 0);
    lv_obj_set_style_border_color(node, color, 0);
}

static void ui_settings_close_editor(ui_settings_t *view)
{
    if (view->editor_layer != NULL) {
        lv_obj_del(view->editor_layer);
        view->editor_layer = NULL;
        view->editor_input = NULL;
        view->editor_keyboard = NULL;
    }
}

static void ui_settings_load_draft(ui_settings_t *view)
{
    settings_manager_get_snapshot(&view->draft);
}

static void ui_settings_apply_field(ui_settings_t *view, const char *text)
{
    if (view->editing_field == SETTINGS_FIELD_UNLOCK) {
        if (settings_manager_verify_pin(text)) {
            view->unlocked = true;
            ui_settings_load_draft(view);
            ui_settings_set_status(view, "管理员已解锁");
        } else {
            ui_settings_set_status(view, "PIN 错误");
        }
        return;
    }

    switch (view->editing_field) {
    case SETTINGS_FIELD_SSID:
        strlcpy(view->draft.wifi_ssid, text, sizeof(view->draft.wifi_ssid));
        break;
    case SETTINGS_FIELD_WIFI_PASSWORD:
        if (text[0] != '\0') {
            strlcpy(view->draft.wifi_password, text, sizeof(view->draft.wifi_password));
        }
        break;
    case SETTINGS_FIELD_EDGE_URL:
        strlcpy(view->draft.edge_api_url, text, sizeof(view->draft.edge_api_url));
        break;
    case SETTINGS_FIELD_MODEL:
        strlcpy(view->draft.llm_model, text, sizeof(view->draft.llm_model));
        break;
    case SETTINGS_FIELD_DEVICE_ID:
        strlcpy(view->draft.device_id, text, sizeof(view->draft.device_id));
        break;
    case SETTINGS_FIELD_EDGE_KEY:
        if (text[0] != '\0') {
            strlcpy(view->draft.edge_key, text, sizeof(view->draft.edge_key));
        }
        break;
    case SETTINGS_FIELD_INTERVAL:
        view->draft.ai_auto_interval_seconds = (uint32_t)strtoul(text, NULL, 10);
        break;
    case SETTINGS_FIELD_ADMIN_PIN:
        if (text[0] != '\0') {
            char error[80];
            if (settings_manager_save_pin(text, error, sizeof(error)) == ESP_OK) {
                ui_settings_load_draft(view);
                ui_settings_set_status(view, "PIN 已保存");
            } else {
                ui_settings_set_status(view, error);
            }
        }
        return;
    case SETTINGS_FIELD_FORECAST_URL:
        strlcpy(view->draft.forecast_api_url, text, sizeof(view->draft.forecast_api_url));
        break;
    case SETTINGS_FIELD_FORECAST_KEY:
        if (text[0] != '\0') strlcpy(view->draft.forecast_key, text, sizeof(view->draft.forecast_key));
        break;
    case SETTINGS_FIELD_FORECAST_LOCATION:
        strlcpy(view->draft.location_name, text, sizeof(view->draft.location_name));
        break;
    case SETTINGS_FIELD_FORECAST_LATITUDE:
        view->draft.latitude = strtof(text, NULL);
        break;
    case SETTINGS_FIELD_FORECAST_LONGITUDE:
        view->draft.longitude = strtof(text, NULL);
        break;
    case SETTINGS_FIELD_FORECAST_TIMEZONE:
        strlcpy(view->draft.timezone, text, sizeof(view->draft.timezone));
        break;
    case SETTINGS_FIELD_FORECAST_INTERVAL: {
        char error[80];
        view->draft.forecast_refresh_seconds = (uint32_t)strtoul(text, NULL, 10);
        if (settings_manager_save_forecast(&view->draft, error, sizeof(error)) == ESP_OK) {
            ui_settings_load_draft(view);
            forecast_client_request_refresh();
            ui_settings_set_status(view, "预测配置已保存，正在刷新");
        } else {
            ui_settings_set_status(view, error);
        }
        return;
    }
    default:
        break;
    }
    ui_settings_set_status(view, "修改待保存");
}

static void ui_settings_commit_editor(ui_settings_t *view)
{
    settings_field_t deferred = SETTINGS_FIELD_UNLOCK;
    settings_field_t next_forecast = SETTINGS_FIELD_UNLOCK;

    if (view->editing_field == SETTINGS_FIELD_UNLOCK &&
        !settings_manager_verify_pin(lv_textarea_get_text(view->editor_input))) {
        lv_textarea_set_text(view->editor_input, "");
        lv_label_set_text(view->editor_hint, "PIN 错误，请重新输入");
        ui_settings_set_status(view, "PIN 错误");
        return;
    }

    next_forecast = ui_settings_next_forecast_field(view->editing_field);
    ui_settings_apply_field(view, lv_textarea_get_text(view->editor_input));
    if (view->editing_field == SETTINGS_FIELD_UNLOCK && view->unlocked &&
        view->deferred_field != SETTINGS_FIELD_UNLOCK) {
        deferred = view->deferred_field;
    }
    view->deferred_field = SETTINGS_FIELD_UNLOCK;
    ui_settings_close_editor(view);
    if (deferred != SETTINGS_FIELD_UNLOCK) {
        ui_settings_show_editor(view, deferred);
    } else if (next_forecast != SETTINGS_FIELD_UNLOCK) {
        ui_settings_show_editor(view, next_forecast);
    }
}

static void ui_settings_editor_confirm_event(lv_event_t *event)
{
    ui_settings_t *view = lv_event_get_user_data(event);

    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        ui_settings_commit_editor(view);
    }
}

static void ui_settings_editor_cancel_event(lv_event_t *event)
{
    ui_settings_t *view = lv_event_get_user_data(event);

    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        ui_settings_close_editor(view);
    }
}

static void ui_settings_matrix_event(lv_event_t *event)
{
    ui_settings_t *view = lv_event_get_user_data(event);
    lv_obj_t *matrix = lv_event_get_target(event);
    const uint32_t *button_id = lv_event_get_param(event);
    const char *key;
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;
    if (button_id == NULL || *button_id > UINT16_MAX) return;
    key = lv_btnmatrix_get_btn_text(matrix, (uint16_t)*button_id);
    if (key == NULL) return;
    if (strcmp(key, "OK") == 0) { ui_settings_commit_editor(view); return; }
    if (strcmp(key, "CANCEL") == 0) { ui_settings_close_editor(view); return; }
    if (strcmp(key, "BK") == 0) { lv_textarea_del_char(view->editor_input); return; }
    if (strcmp(key, "CLR") == 0) { lv_textarea_set_text(view->editor_input, ""); return; }
    if (strcmp(key, "SPACE") == 0) { lv_textarea_add_char(view->editor_input, ' '); return; }
    if (strcmp(key, "CASE") == 0) {
        view->editor_upper = !view->editor_upper;
        lv_btnmatrix_set_map(matrix, view->editor_upper ? s_ascii_upper : s_ascii_lower);
        return;
    }
    lv_textarea_add_text(view->editor_input, key);
}

static void ui_settings_show_editor(ui_settings_t *view, settings_field_t field)
{
    lv_obj_t *panel;
    lv_obj_t *cancel;
    lv_obj_t *confirm;
    const char *initial = "";
    char number_buffer[20];
    bool numeric = field == SETTINGS_FIELD_UNLOCK || field == SETTINGS_FIELD_INTERVAL || field == SETTINGS_FIELD_ADMIN_PIN ||
                   field == SETTINGS_FIELD_FORECAST_LATITUDE || field == SETTINGS_FIELD_FORECAST_LONGITUDE ||
                   field == SETTINGS_FIELD_FORECAST_INTERVAL;
    bool secret = field == SETTINGS_FIELD_UNLOCK || field == SETTINGS_FIELD_WIFI_PASSWORD ||
                  field == SETTINGS_FIELD_EDGE_KEY || field == SETTINGS_FIELD_ADMIN_PIN || field == SETTINGS_FIELD_FORECAST_KEY;

    if (view->editor_layer != NULL) {
        return;
    }
    view->editing_field = field;
    if (field == SETTINGS_FIELD_SSID) {
        initial = view->draft.wifi_ssid;
    } else if (field == SETTINGS_FIELD_EDGE_URL) {
        initial = view->draft.edge_api_url;
    } else if (field == SETTINGS_FIELD_MODEL) {
        initial = view->draft.llm_model;
    } else if (field == SETTINGS_FIELD_DEVICE_ID) {
        initial = view->draft.device_id;
    } else if (field == SETTINGS_FIELD_INTERVAL) {
        static char interval[12];
        (void)snprintf(interval, sizeof(interval), "%lu", (unsigned long)view->draft.ai_auto_interval_seconds);
        initial = interval;
    } else if (field == SETTINGS_FIELD_FORECAST_URL) {
        initial = view->draft.forecast_api_url;
    } else if (field == SETTINGS_FIELD_FORECAST_LOCATION) {
        initial = view->draft.location_name;
    } else if (field == SETTINGS_FIELD_FORECAST_LATITUDE) {
        (void)snprintf(number_buffer, sizeof(number_buffer), "%.6f", (double)view->draft.latitude);
        initial = number_buffer;
    } else if (field == SETTINGS_FIELD_FORECAST_LONGITUDE) {
        (void)snprintf(number_buffer, sizeof(number_buffer), "%.6f", (double)view->draft.longitude);
        initial = number_buffer;
    } else if (field == SETTINGS_FIELD_FORECAST_TIMEZONE) {
        initial = view->draft.timezone;
    } else if (field == SETTINGS_FIELD_FORECAST_INTERVAL) {
        (void)snprintf(number_buffer, sizeof(number_buffer), "%lu", (unsigned long)view->draft.forecast_refresh_seconds);
        initial = number_buffer;
    }

    view->editor_layer = lv_obj_create(lv_layer_top());
    lv_obj_set_pos(view->editor_layer, 0, 0);
    lv_obj_set_size(view->editor_layer, 800, 480);
    lv_obj_set_style_bg_color(view->editor_layer, lv_color_hex(0x101513), 0);
    lv_obj_set_style_bg_opa(view->editor_layer, LV_OPA_80, 0);
    lv_obj_set_style_border_width(view->editor_layer, 0, 0);
    lv_obj_set_style_pad_all(view->editor_layer, 0, 0);
    lv_obj_clear_flag(view->editor_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(view->editor_layer, LV_OBJ_FLAG_FLOATING);
    panel = lv_obj_create(view->editor_layer);
    lv_obj_set_pos(panel, 80, 12);
    lv_obj_set_size(panel, 640, 164);
    lv_obj_set_style_bg_color(panel, lv_color_hex(UI_SURFACE_COLOR), 0);
    lv_obj_set_style_radius(panel, UI_CARD_RADIUS, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(UI_BORDER_COLOR), 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    view->editor_title = ui_theme_label_create(panel, ui_settings_field_name(field), 20, 14,
                                                &panel_zh_20, lv_color_hex(UI_TEXT_COLOR));
    view->editor_input = lv_textarea_create(panel);
    lv_obj_set_pos(view->editor_input, 20, 50);
    lv_obj_set_size(view->editor_input, 600, 50);
    lv_obj_set_style_text_font(view->editor_input, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(view->editor_input, lv_color_hex(UI_TEXT_COLOR), 0);
    lv_obj_set_style_bg_color(view->editor_input, lv_color_hex(0x202325), 0);
    lv_obj_set_style_bg_opa(view->editor_input, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(view->editor_input, 4, 0);
    lv_obj_set_style_border_color(view->editor_input, lv_color_hex(UI_ACCENT_COLOR), 0);
    lv_textarea_set_one_line(view->editor_input, true);
    lv_textarea_set_max_length(view->editor_input,
                               (field == SETTINGS_FIELD_EDGE_URL || field == SETTINGS_FIELD_FORECAST_URL) ? 180 :
                               ((field == SETTINGS_FIELD_EDGE_KEY || field == SETTINGS_FIELD_FORECAST_KEY) ? 127 : 64));
    lv_textarea_set_password_mode(view->editor_input, secret);
    lv_textarea_set_text(view->editor_input, initial);
    lv_textarea_set_cursor_pos(view->editor_input, LV_TEXTAREA_CURSOR_LAST);
    lv_obj_add_state(view->editor_input, LV_STATE_FOCUSED);
    view->editor_hint = ui_theme_label_create(panel,
                                               secret ? "留空保持原值，完成请按确认" : "完成请按确认",
                                               20, 116, &panel_zh_20, lv_color_hex(UI_MUTED_COLOR));
    cancel = lv_btn_create(panel);
    lv_obj_set_pos(cancel, 418, 112);
    lv_obj_set_size(cancel, 92, 38);
    lv_obj_set_style_bg_color(cancel, lv_color_hex(UI_SURFACE_MUTED), 0);
    lv_obj_t *cancel_text = lv_label_create(cancel);
    lv_label_set_text(cancel_text, "取消");
    lv_obj_set_style_text_font(cancel_text, &panel_zh_20, 0);
    lv_obj_center(cancel_text);
    lv_obj_add_event_cb(cancel, ui_settings_editor_cancel_event, LV_EVENT_CLICKED, view);
    confirm = lv_btn_create(panel);
    lv_obj_set_pos(confirm, 528, 112);
    lv_obj_set_size(confirm, 92, 38);
    lv_obj_set_style_bg_color(confirm, lv_color_hex(UI_ACCENT_COLOR), 0);
    lv_obj_set_style_border_width(confirm, 0, 0);
    lv_obj_t *confirm_text = lv_label_create(confirm);
    lv_label_set_text(confirm_text, "确认");
    lv_obj_set_style_text_font(confirm_text, &panel_zh_20, 0);
    lv_obj_set_style_text_color(confirm_text, lv_color_white(), 0);
    lv_obj_center(confirm_text);
    lv_obj_add_event_cb(confirm, ui_settings_editor_confirm_event, LV_EVENT_CLICKED, view);
    view->editor_upper = false;
    view->editor_keyboard = lv_btnmatrix_create(view->editor_layer);
    lv_obj_set_pos(view->editor_keyboard, numeric ? 200 : 8, 188);
    lv_obj_set_size(view->editor_keyboard, numeric ? 400 : 784, 284);
    lv_obj_set_style_text_font(view->editor_keyboard, &lv_font_montserrat_16, 0);
    lv_obj_set_style_bg_color(view->editor_keyboard, lv_color_hex(0x1B1D1E), 0);
    lv_obj_set_style_pad_all(view->editor_keyboard, 5, 0);
    lv_obj_set_style_pad_row(view->editor_keyboard, 5, 0);
    lv_obj_set_style_pad_column(view->editor_keyboard, 5, 0);
    lv_obj_set_style_bg_color(view->editor_keyboard, lv_color_hex(UI_SURFACE_MUTED), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(view->editor_keyboard, lv_color_hex(UI_ACCENT_COLOR), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(view->editor_keyboard, lv_color_white(), LV_PART_ITEMS);
    lv_btnmatrix_set_map(view->editor_keyboard, numeric ? s_number_keys : s_ascii_lower);
    lv_obj_add_event_cb(view->editor_keyboard, ui_settings_matrix_event, LV_EVENT_VALUE_CHANGED, view);
}

static void ui_settings_field_event(lv_event_t *event)
{
    ui_settings_t *view = lv_event_get_user_data(event);
    settings_field_t field = (settings_field_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_target(event));
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    if (!view->unlocked && field != SETTINGS_FIELD_UNLOCK) {
        view->deferred_field = field;
        ui_settings_set_status(view, "请先管理员解锁");
        ui_settings_show_editor(view, SETTINGS_FIELD_UNLOCK);
        return;
    }
    ui_settings_show_editor(view, field);
}

static void ui_settings_action_event(lv_event_t *event)
{
    ui_settings_t *view = lv_event_get_user_data(event);
    uintptr_t action = (uintptr_t)lv_obj_get_user_data(lv_event_get_target(event));
    char error[80];
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    if (!view->unlocked && (action == 1U || action == 2U || action == 3U || action == 6U)) {
        ui_settings_set_status(view, "请先管理员解锁");
        ui_settings_show_editor(view, SETTINGS_FIELD_UNLOCK);
        return;
    }
    if (action == 1U) {
        if (settings_manager_save_network(&view->draft, error, sizeof(error)) == ESP_OK) {
            (void)wifi_manager_apply_settings();
            ui_settings_set_status(view, "已保存，正在重新连接");
        } else {
            ui_settings_set_status(view, error);
        }
    } else if (action == 2U) {
        static const char *buttons[] = {"确认", "取消", ""};
        if (view->restore_confirm == NULL) {
            view->restore_confirm = lv_msgbox_create(NULL, "恢复默认", "确认清除本地配置并恢复默认值", buttons, true);
            lv_obj_set_style_text_font(view->restore_confirm, &panel_zh_20, 0);
            lv_obj_set_width(view->restore_confirm, 420);
            lv_obj_center(view->restore_confirm);
            lv_obj_add_event_cb(view->restore_confirm, ui_settings_action_event, LV_EVENT_VALUE_CHANGED, view);
            lv_obj_set_user_data(view->restore_confirm, (void *)5U);
        }
    } else if (action == 3U) {
        view->unlocked = false;
        ui_settings_set_status(view, "设置已锁定");
    } else if (action == 6U) {
        if (settings_manager_save_edge(&view->draft, error, sizeof(error)) == ESP_OK) {
            ui_settings_set_status(view, "边缘服务已保存");
        } else {
            ui_settings_set_status(view, error);
        }
    } else if (action == 7U) {
        if (!view->unlocked) {
            view->deferred_field = SETTINGS_FIELD_FORECAST_URL;
            ui_settings_show_editor(view, SETTINGS_FIELD_UNLOCK);
        } else {
            ui_settings_show_editor(view, SETTINGS_FIELD_FORECAST_URL);
        }
    } else if (action == 4U) {
        (void)wifi_manager_reconnect();
        ui_settings_set_status(view, "正在重新连接 Wi-Fi");
    } else if (action == 5U) {
        lv_obj_t *box = lv_event_get_target(event);
        if (lv_msgbox_get_active_btn(box) == 0U && settings_manager_restore_defaults() == ESP_OK) {
            ui_settings_load_draft(view);
            (void)wifi_manager_apply_settings();
            ui_settings_set_status(view, "已恢复默认配置");
        }
        lv_msgbox_close(box);
        view->restore_confirm = NULL;
    }
}

static lv_obj_t *ui_settings_button(lv_obj_t *parent, int x, int y, int width, int height,
                                    const char *text, lv_color_t color, lv_event_cb_t callback,
                                    void *user_data)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_t *label = lv_label_create(button);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_color(button, color, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(button, 4, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(button, LV_STATE_DISABLED);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &panel_zh_20, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
    return button;
}

static void ui_settings_add_field(ui_settings_t *view, lv_obj_t *parent, unsigned index,
                                  int y, const char *title, settings_field_t field)
{
    view->field_buttons[index] = ui_settings_button(parent, 12, y, 306, 38,
                                                     title, lv_color_hex(UI_ACCENT_COLOR),
                                                     ui_settings_field_event, view);
    lv_obj_set_style_bg_opa(view->field_buttons[index], LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(view->field_buttons[index], lv_color_hex(UI_SURFACE_MUTED), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(view->field_buttons[index], 2, 0);
    lv_obj_set_style_border_color(view->field_buttons[index], lv_color_hex(UI_BORDER_COLOR), 0);
    lv_obj_set_style_shadow_width(view->field_buttons[index], 0, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(view->field_buttons[index], 0), lv_color_white(), 0);
    lv_obj_move_foreground(view->field_buttons[index]);
    lv_obj_set_user_data(view->field_buttons[index], (void *)(uintptr_t)field);
}

ui_settings_t *ui_settings_create(lv_obj_t *parent)
{
    static lv_point_t lines[3][2] = {
        {{106, 54}, {174, 54}}, {{276, 54}, {344, 54}}, {{446, 54}, {514, 54}},
    };
    static const char *node_names[] = {"RS485", "WIFI", "EDGE", "AI"};
    lv_obj_t *card;
    ui_settings_t *view = lv_mem_alloc(sizeof(*view));
    if (view == NULL) {
        return NULL;
    }
    memset(view, 0, sizeof(*view));
    ui_settings_load_draft(view);
    ui_settings_set_status(view, "设置已锁定");

    ui_theme_label_create(parent, "终端设置", 14, 10, &panel_zh_20, lv_color_white());
    ui_theme_label_create(parent, "AIoT SETTINGS", 526, 12, &lv_font_montserrat_16, lv_color_hex(UI_MUTED_COLOR));
    for (unsigned index = 0; index < 3U; index++) {
        lv_obj_t *line = lv_line_create(parent);
        lv_line_set_points(line, lines[index], 2);
        lv_obj_set_style_line_width(line, 2, 0);
        lv_obj_set_style_line_color(line, lv_color_hex(UI_BORDER_COLOR), 0);
    }
    for (unsigned index = 0; index < 4U; index++) {
        view->link_nodes[index] = lv_obj_create(parent);
        lv_obj_set_pos(view->link_nodes[index], 14 + (int)index * 170, 36);
        lv_obj_set_size(view->link_nodes[index], 92, 36);
        lv_obj_set_style_radius(view->link_nodes[index], 4, 0);
        lv_obj_set_style_border_width(view->link_nodes[index], 0, 0);
        lv_obj_clear_flag(view->link_nodes[index], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *label = lv_label_create(view->link_nodes[index]);
        lv_label_set_text(label, node_names[index]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_center(label);
    }
    view->link_status = ui_theme_label_create(parent, "设备链路状态", 14, 78, &panel_zh_20, lv_color_hex(0xD8E0DB));
    lv_obj_set_width(view->link_status, 676);
    lv_label_set_long_mode(view->link_status, LV_LABEL_LONG_DOT);

    card = ui_theme_card_create(parent, 14, 98, 330, 300, "网络");
    view->network_info = ui_theme_value_label_create(card, 12, 40, 306);
    lv_obj_set_style_text_font(view->network_info, &lv_font_montserrat_16, 0);
    lv_label_set_long_mode(view->network_info, LV_LABEL_LONG_DOT);
    ui_settings_add_field(view, card, 0, 132, "编辑 Wi-Fi 名称  >", SETTINGS_FIELD_SSID);
    ui_settings_add_field(view, card, 1, 176, "编辑 Wi-Fi 密码  >", SETTINGS_FIELD_WIFI_PASSWORD);
    view->save_button = ui_settings_button(card, 12, 228, 145, 44, "保存网络", lv_color_hex(UI_ACCENT_COLOR),
                                           ui_settings_action_event, view);
    lv_obj_set_user_data(view->save_button, (void *)1U);
    lv_obj_t *reconnect_button = ui_settings_button(card, 173, 228, 145, 44, "重新连接", lv_color_hex(0x53605A),
                                                    ui_settings_action_event, view);
    lv_obj_set_user_data(reconnect_button, (void *)4U);

    card = ui_theme_card_create(parent, 360, 98, 330, 300, "AI 服务");
    view->edge_info = ui_theme_value_label_create(card, 12, 40, 306);
    lv_obj_set_style_text_font(view->edge_info, &lv_font_montserrat_16, 0);
    lv_label_set_long_mode(view->edge_info, LV_LABEL_LONG_DOT);
    lv_obj_add_flag(view->edge_info, LV_OBJ_FLAG_HIDDEN);
    ui_settings_add_field(view, card, 2, 44, "编辑 API 地址  >", SETTINGS_FIELD_EDGE_URL);
    ui_settings_add_field(view, card, 3, 84, "编辑模型名称  >", SETTINGS_FIELD_MODEL);
    ui_settings_add_field(view, card, 4, 124, "编辑设备 ID  >", SETTINGS_FIELD_DEVICE_ID);
    ui_settings_add_field(view, card, 5, 164, "编辑 AI Key  >", SETTINGS_FIELD_EDGE_KEY);
    ui_settings_add_field(view, card, 6, 204, "编辑 AI 周期  >", SETTINGS_FIELD_INTERVAL);
    view->lock_button = ui_settings_button(card, 12, 254, 306, 34, "保存服务", lv_color_hex(UI_ACCENT_COLOR),
                                           ui_settings_action_event, view);
    lv_obj_set_user_data(view->lock_button, (void *)6U);

    card = ui_theme_card_create(parent, 14, 412, 676, 56, "终端安全");
    view->security_info = ui_theme_value_label_create(card, 12, 27, 110);
    lv_obj_set_style_text_font(view->security_info, &panel_zh_20, 0);
    view->unlock_button = ui_settings_button(card, 132, 10, 118, 36, "管理员解锁", lv_color_hex(UI_WARNING_COLOR),
                                              ui_settings_field_event, view);
    lv_obj_set_user_data(view->unlock_button, (void *)(uintptr_t)SETTINGS_FIELD_UNLOCK);
    lv_obj_t *pin_button = ui_settings_button(card, 262, 10, 106, 36, "修改 PIN", lv_color_hex(0x53605A),
                                               ui_settings_field_event, view);
    lv_obj_set_user_data(pin_button, (void *)(uintptr_t)SETTINGS_FIELD_ADMIN_PIN);
    view->restore_button = ui_settings_button(card, 380, 10, 106, 36, "恢复默认", lv_color_hex(UI_DANGER_COLOR),
                                              ui_settings_action_event, view);
    lv_obj_set_user_data(view->restore_button, (void *)2U);
    lv_obj_t *lock_button = ui_settings_button(card, 498, 10, 166, 36, "锁定设置", lv_color_hex(0x53605A),
                                               ui_settings_action_event, view);
    lv_obj_set_user_data(lock_button, (void *)3U);
    lv_obj_t *forecast_button = ui_settings_button(card, 498, 10, 166, 36, "预测位置服务", lv_color_hex(UI_ACCENT_COLOR),
                                                   ui_settings_action_event, view);
    lv_obj_set_user_data(forecast_button, (void *)7U);
    return view;
}

bool ui_settings_needs_refresh(const ui_settings_t *view)
{
    return view != NULL && view->refresh_pending;
}

void ui_settings_update(ui_settings_t *view, const panel_state_snapshot_t *panel,
                        const wifi_manager_snapshot_t *wifi, const ai_state_snapshot_t *ai)
{
    char masked_password[16];
    bool api_configured;
    bool api_online;

    settings_manager_mask_secret(view->draft.wifi_password, masked_password, sizeof(masked_password));
    api_configured = view->draft.edge_api_url[0] != '\0' && view->draft.edge_key[0] != '\0';
    api_online = api_configured && ai->request_status != AI_REQUEST_ERROR;
    ui_settings_set_node(view->link_nodes[0], panel->h743_online, !panel->h743_online && panel->status_valid);
    ui_settings_set_node(view->link_nodes[1], wifi->connected, wifi->configured && !wifi->connected);
    ui_settings_set_node(view->link_nodes[2], api_online, api_configured && !api_online);
    ui_settings_set_node(view->link_nodes[3], ai->request_status == AI_REQUEST_READY,
                         ai->request_status == AI_REQUEST_IN_PROGRESS);
    lv_label_set_text_fmt(view->link_status, "RS485 %s   Wi-Fi %s   Edge %s   %s",
                          panel->h743_online ? "在线" : "离线",
                          wifi->connected ? "在线" : "离线",
                          api_configured ? "已配置" : "未配置", view->status);
    lv_label_set_text_fmt(view->network_info, "SSID   %s\nPASS   %s\nRSSI   %d dBm\nIP     %s",
                          view->draft.wifi_ssid[0] != '\0' ? view->draft.wifi_ssid : "--",
                          masked_password, wifi->rssi, wifi->ip[0] != '\0' ? wifi->ip : "--");
    (void)ai;
    lv_label_set_text(view->security_info, view->unlocked ? "状态\n已解锁" : "状态\n已锁定");
    view->refresh_pending = false;
}
