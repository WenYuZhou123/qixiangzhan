#include "ui_pad_gate.h"

#include "ui_theme.h"

LV_FONT_DECLARE(panel_zh_20);

#define GATE_POSE_UNKNOWN (-1)
#define GATE_POSE_CLOSED  0
#define GATE_POSE_OPEN    1

static int16_t ui_pad_gate_left_x(int8_t pose)
{
    return (pose == GATE_POSE_OPEN) ? 0 : 64;
}

static int16_t ui_pad_gate_right_x(int8_t pose)
{
    return (pose == GATE_POSE_OPEN) ? 242 : 178;
}

static void ui_pad_gate_set_x_exec(void *object, int32_t value)
{
    lv_obj_set_x(object, (lv_coord_t)value);
}

static void ui_pad_gate_anim_x(lv_obj_t *object, int16_t from, int16_t to, uint16_t duration)
{
    lv_anim_t animation;

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, object);
    lv_anim_set_values(&animation, from, to);
    lv_anim_set_time(&animation, duration);
    lv_anim_set_exec_cb(&animation, ui_pad_gate_set_x_exec);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);
}

static void ui_pad_gate_flash_exec(void *object, int32_t value)
{
    lv_obj_set_style_bg_opa(object, (lv_opa_t)value, 0);
}

static void ui_pad_gate_flash(ui_pad_gate_t *view, lv_color_t color)
{
    lv_anim_t animation;

    lv_obj_set_style_bg_color(view->flash_strip, color, 0);
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, view->flash_strip);
    lv_anim_set_values(&animation, LV_OPA_TRANSP, LV_OPA_90);
    lv_anim_set_time(&animation, 120);
    lv_anim_set_playback_time(&animation, 260);
    lv_anim_set_repeat_count(&animation, 1);
    lv_anim_set_exec_cb(&animation, ui_pad_gate_flash_exec);
    lv_anim_start(&animation);
}

static void ui_pad_gate_set_pose(ui_pad_gate_t *view, int8_t pose, bool animate)
{
    int16_t left = ui_pad_gate_left_x(pose);
    int16_t right = ui_pad_gate_right_x(pose);

    if (animate && view->pose != GATE_POSE_UNKNOWN) {
        ui_pad_gate_anim_x(view->left_door, lv_obj_get_x(view->left_door), left, 650);
        ui_pad_gate_anim_x(view->right_door, lv_obj_get_x(view->right_door), right, 650);
    } else {
        lv_obj_set_x(view->left_door, left);
        lv_obj_set_x(view->right_door, right);
    }
    view->pose = pose;
}

static int8_t ui_pad_gate_snapshot_pose(const panel_state_snapshot_t *state)
{
    if (state->relay1_on && state->relay2_on) {
        return GATE_POSE_OPEN;
    }
    if (!state->relay1_on && !state->relay2_on) {
        return GATE_POSE_CLOSED;
    }
    return GATE_POSE_UNKNOWN;
}

bool ui_pad_gate_is_pending(const panel_state_snapshot_t *state)
{
    return state->pending_command != PANEL_COMMAND_NONE;
}

void ui_pad_gate_init(ui_pad_gate_t *view, lv_obj_t *parent, int x, int y, int width, int height)
{
    lv_obj_t *stage;
    lv_obj_t *marker;

    view->card = ui_theme_card_create(parent, x, y, width, height, "停机坪状态");
    stage = lv_obj_create(view->card);
    lv_obj_set_pos(stage, 14, 42);
    lv_obj_set_size(stage, width - 28, 148);
    lv_obj_set_style_bg_color(stage, lv_color_hex(0x24282A), 0);
    lv_obj_set_style_border_color(stage, lv_color_hex(UI_BORDER_COLOR), 0);
    lv_obj_set_style_border_width(stage, 1, 0);
    lv_obj_set_style_radius(stage, 4, 0);
    lv_obj_set_style_pad_all(stage, 0, 0);
    lv_obj_clear_flag(stage, LV_OBJ_FLAG_SCROLLABLE);
    marker = ui_theme_label_create(stage, "H", 150, 52, &lv_font_montserrat_20, lv_color_hex(UI_ACCENT_COLOR));
    lv_obj_set_style_text_letter_space(marker, 0, 0);
    view->left_door = lv_obj_create(stage);
    lv_obj_set_pos(view->left_door, 64, 18);
    lv_obj_set_size(view->left_door, 100, 112);
    lv_obj_set_style_bg_color(view->left_door, lv_color_hex(0x4A4E52), 0);
    lv_obj_set_style_border_color(view->left_door, lv_color_hex(0x7A8085), 0);
    lv_obj_set_style_border_width(view->left_door, 2, 0);
    lv_obj_set_style_radius(view->left_door, 3, 0);
    view->right_door = lv_obj_create(stage);
    lv_obj_set_pos(view->right_door, 178, 18);
    lv_obj_set_size(view->right_door, 100, 112);
    lv_obj_set_style_bg_color(view->right_door, lv_color_hex(0x4A4E52), 0);
    lv_obj_set_style_border_color(view->right_door, lv_color_hex(0x7A8085), 0);
    lv_obj_set_style_border_width(view->right_door, 2, 0);
    lv_obj_set_style_radius(view->right_door, 3, 0);
    view->flash_strip = lv_obj_create(stage);
    lv_obj_set_pos(view->flash_strip, 0, 140);
    lv_obj_set_size(view->flash_strip, width - 28, 8);
    lv_obj_set_style_bg_opa(view->flash_strip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view->flash_strip, 0, 0);
    view->state_label = ui_theme_label_create(view->card, "状态未更新", 14, 202, &panel_zh_20, lv_color_hex(UI_MUTED_COLOR));
    view->channel_label = ui_theme_label_create(view->card, "左通道 --   右通道 --", 14, 230, &panel_zh_20, lv_color_hex(UI_MUTED_COLOR));
    view->pose = GATE_POSE_UNKNOWN;
    view->observed_completion_id = 0U;
    (void)height;
}

void ui_pad_gate_update(ui_pad_gate_t *view, const panel_state_snapshot_t *state)
{
    bool current = state->h743_online && state->status_valid;
    int8_t target = ui_pad_gate_snapshot_pose(state);
    bool completed = state->command_completion_id != 0U &&
                     state->command_completion_id != view->observed_completion_id;

    if (!current) {
        lv_label_set_text(view->state_label,
                          state->command_timed_out ? "链路超时" : "状态未更新");
        lv_obj_set_style_text_color(view->state_label, lv_color_hex(UI_DANGER_COLOR), 0);
        return;
    }
    lv_label_set_text_fmt(view->channel_label,
                          "左通道 %s   右通道 %s",
                          state->relay1_on ? "已开启" : "已关闭",
                          state->relay2_on ? "已开启" : "已关闭");
    if (ui_pad_gate_is_pending(state)) {
        lv_label_set_text(view->state_label, "命令发送中");
        lv_obj_set_style_text_color(view->state_label, lv_color_hex(UI_WARNING_COLOR), 0);
        return;
    }
    if (completed) {
        view->observed_completion_id = state->command_completion_id;
        if (state->last_command_result == PANEL_COMMAND_RESULT_SUCCESS && target != GATE_POSE_UNKNOWN) {
            bool is_stop = state->completed_command == PANEL_COMMAND_PAD_STOP;

            ui_pad_gate_set_pose(view, target, true);
            lv_label_set_text(view->state_label, is_stop ? "关闭" : (target == GATE_POSE_OPEN ? "开启" : "关闭"));
            lv_obj_set_style_text_color(view->state_label, lv_color_hex(is_stop ? UI_DANGER_COLOR : UI_ONLINE_COLOR), 0);
            if (is_stop) {
                ui_pad_gate_flash(view, lv_color_hex(UI_DANGER_COLOR));
            }
            return;
        }
        lv_label_set_text(view->state_label, "执行失败");
        lv_obj_set_style_text_color(view->state_label, lv_color_hex(UI_DANGER_COLOR), 0);
        ui_pad_gate_flash(view, lv_color_hex(UI_DANGER_COLOR));
        return;
    }
    if (target == GATE_POSE_UNKNOWN) {
        lv_label_set_text(view->state_label, "状态未更新");
        lv_obj_set_style_text_color(view->state_label, lv_color_hex(UI_WARNING_COLOR), 0);
        return;
    }
    ui_pad_gate_set_pose(view, target, view->pose != GATE_POSE_UNKNOWN && view->pose != target);
    lv_label_set_text(view->state_label, target == GATE_POSE_OPEN ? "开启" : "关闭");
    lv_obj_set_style_text_color(view->state_label, lv_color_hex(UI_ONLINE_COLOR), 0);
}
