#include "ui_wind.h"

#include "ui_theme.h"

LV_FONT_DECLARE(panel_zh_20);

static void ui_wind_set_visible(ui_wind_t *view, bool visible)
{
    int index;

    if (visible) {
        lv_obj_clear_flag(view->needle, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view->offline_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(view->needle, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(view->offline_label, LV_OBJ_FLAG_HIDDEN);
    }
    for (index = 0; index < 3; index++) {
        if (visible) {
            lv_obj_clear_flag(view->wind_lines[index], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(view->wind_lines[index], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void ui_wind_init(ui_wind_t *view, lv_obj_t *parent, int x, int y, int width, int height)
{
    lv_obj_t *compass;
    lv_obj_t *speed_icon;
    int index;

    view->card = ui_theme_card_create(parent, x, y, width, height, "风场");
    compass = lv_obj_create(view->card);
    lv_obj_set_pos(compass, 18, 42);
    lv_obj_set_size(compass, 144, 144);
    lv_obj_set_style_radius(compass, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(compass, lv_color_hex(UI_SURFACE_MUTED), 0);
    lv_obj_set_style_border_color(compass, lv_color_hex(UI_ACCENT_COLOR), 0);
    lv_obj_set_style_border_width(compass, 2, 0);
    lv_obj_set_style_pad_all(compass, 0, 0);
    lv_obj_clear_flag(compass, LV_OBJ_FLAG_SCROLLABLE);
    ui_theme_label_create(compass, "N", 62, 5, &lv_font_montserrat_14, lv_color_hex(UI_TEXT_COLOR));
    ui_theme_label_create(compass, "E", 124, 64, &lv_font_montserrat_14, lv_color_hex(UI_TEXT_COLOR));
    ui_theme_label_create(compass, "S", 64, 122, &lv_font_montserrat_14, lv_color_hex(UI_TEXT_COLOR));
    ui_theme_label_create(compass, "W", 5, 64, &lv_font_montserrat_14, lv_color_hex(UI_TEXT_COLOR));
    view->needle_points[0] = (lv_point_t){ 72, 72 };
    view->needle_points[1] = (lv_point_t){ 72, 20 };
    view->needle = lv_line_create(compass);
    lv_line_set_points(view->needle, view->needle_points, 2);
    lv_obj_set_size(view->needle, 144, 144);
    lv_obj_set_style_line_color(view->needle, lv_color_hex(UI_DANGER_COLOR), 0);
    lv_obj_set_style_line_width(view->needle, 5, 0);
    lv_obj_set_style_line_rounded(view->needle, true, 0);
    lv_obj_set_style_transform_pivot_x(view->needle, 72, 0);
    lv_obj_set_style_transform_pivot_y(view->needle, 72, 0);

    speed_icon = lv_obj_create(view->card);
    lv_obj_set_pos(speed_icon, 192, 62);
    lv_obj_set_size(speed_icon, 122, 82);
    lv_obj_set_style_bg_opa(speed_icon, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(speed_icon, 0, 0);
    lv_obj_set_style_pad_all(speed_icon, 0, 0);
    lv_obj_clear_flag(speed_icon, LV_OBJ_FLAG_SCROLLABLE);
    for (index = 0; index < 3; index++) {
        view->wind_lines[index] = lv_obj_create(speed_icon);
        lv_obj_set_pos(view->wind_lines[index], 4 + index * 10, 8 + index * 24);
        lv_obj_set_size(view->wind_lines[index], 50, 7);
        lv_obj_set_style_radius(view->wind_lines[index], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(view->wind_lines[index], lv_color_hex(UI_ACCENT_COLOR), 0);
        lv_obj_set_style_border_width(view->wind_lines[index], 0, 0);
    }
    view->speed_label = ui_theme_label_create(view->card, "-- m/s", 184, 156, &lv_font_montserrat_20, lv_color_hex(UI_TEXT_COLOR));
    view->direction_label = ui_theme_label_create(view->card, "--- deg", 184, 196, &lv_font_montserrat_20, lv_color_hex(UI_MUTED_COLOR));
    view->offline_label = ui_theme_label_create(view->card, "传感器离线", 202, 112, &panel_zh_20, lv_color_hex(UI_DANGER_COLOR));
    lv_obj_add_flag(view->offline_label, LV_OBJ_FLAG_HIDDEN);
    (void)height;
}

void ui_wind_update(ui_wind_t *view, const panel_state_snapshot_t *state)
{
    static const lv_opa_t line_opacity[] = { LV_OPA_50, LV_OPA_70, LV_OPA_COVER };
    char speed[20];
    char direction[20];
    int index;
    int width;

    ui_wind_set_visible(view, state->wind_online);
    if (!state->wind_online) {
        lv_label_set_text(view->speed_label, "-- m/s");
        lv_label_set_text(view->direction_label, "--- deg");
        return;
    }
    lv_obj_set_style_transform_angle(view->needle, (int32_t)(state->wind_direction_deg * 10.0f), 0);
    ui_theme_format_decimal(speed, sizeof(speed), state->wind_speed_mps, 1U);
    ui_theme_format_decimal(direction, sizeof(direction), state->wind_direction_deg, 0U);
    lv_label_set_text_fmt(view->speed_label, "%s m/s", speed);
    lv_label_set_text_fmt(view->direction_label, "%s deg", direction);
    for (index = 0; index < 3; index++) {
        width = 26 + (int)(state->wind_speed_mps * (float)(index + 2));
        if (width > 100) {
            width = 100;
        }
        lv_obj_set_width(view->wind_lines[index], width);
        lv_obj_set_style_bg_opa(view->wind_lines[index], line_opacity[index], 0);
    }
}
