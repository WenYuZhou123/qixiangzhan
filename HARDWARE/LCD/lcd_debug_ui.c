#include "lcd_debug_ui.h"

#include "app_main.h"
#include "key.h"
#include "lcd_port.h"
#include "lcd_touch.h"
#include "lcd_ui_model.h"
#include "lcd_ui_pages.h"
#include "usart.h"
#include <string.h>

static LCD_Page_t s_page = LCD_PAGE_OVERVIEW;
static LCD_UI_Model_t s_model;
static uint32_t s_last_refresh_tick = 0U;
static uint32_t s_last_render_tick = 0U;
static uint32_t s_refresh_count = 0U;
static uint8_t s_force_render = 1U;
static uint8_t s_has_last_status = 0U;
static App_DeviceStatus_t s_work_status;
static App_DeviceStatus_t s_last_status;
static uint8_t s_touch_active = 0U;
static uint16_t s_touch_start_x = 0U;
static uint16_t s_touch_start_y = 0U;

#define LCD_TOUCH_SWIPE_THRESHOLD     60U
#define LCD_TOUCH_TAP_THRESHOLD       20U

static void lcd_diag_print(const char *text)
{
    (void)text;
}

static uint16_t lcd_abs_diff_u16(uint16_t a, uint16_t b)
{
    return (a >= b) ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

static float lcd_abs_diff_f(float a, float b)
{
    float diff = a - b;
    return (diff < 0.0f) ? -diff : diff;
}

static uint8_t lcd_text_changed(const char *a, const char *b)
{
    if ((a == NULL) || (b == NULL))
    {
        return (uint8_t)((a != b) ? 1U : 0U);
    }

    return (uint8_t)((strcmp(a, b) != 0) ? 1U : 0U);
}

static uint16_t lcd_abs_diff_u16_simple(uint16_t a, uint16_t b)
{
    return (a >= b) ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

static void lcd_change_page_relative(int8_t delta)
{
    int16_t next_page;

    next_page = (int16_t)s_page + delta;
    if (next_page < 0)
    {
        next_page = (int16_t)LCD_PAGE_COUNT - 1;
    }
    else if (next_page >= (int16_t)LCD_PAGE_COUNT)
    {
        next_page = 0;
    }

    s_page = (LCD_Page_t)next_page;
    s_force_render = 1U;
}

static uint8_t lcd_try_select_page_tab(uint16_t x, uint16_t y)
{
    uint16_t tab_x;
    uint8_t index;

    if (y < LCD_UI_TAB_Y || y >= (uint16_t)(LCD_UI_TAB_Y + LCD_UI_TAB_H))
    {
        return 0U;
    }

    tab_x = LCD_UI_TAB_X;
    for (index = 0U; index < LCD_PAGE_COUNT; index++)
    {
        if (x >= tab_x && x < (uint16_t)(tab_x + LCD_UI_TAB_W))
        {
            s_page = (LCD_Page_t)index;
            s_force_render = 1U;
            return 1U;
        }
        tab_x = (uint16_t)(tab_x + LCD_UI_TAB_W + LCD_UI_TAB_GAP);
    }

    return 0U;
}

static void lcd_handle_touch_navigation(void)
{
    LCD_TouchState_t touch_state;
    uint16_t dx;
    uint16_t dy;

    if (LCD_Touch_Read(&touch_state) != 0U && touch_state.pressed != 0U)
    {
        if (s_touch_active == 0U)
        {
            s_touch_active = 1U;
            s_touch_start_x = touch_state.x;
            s_touch_start_y = touch_state.y;
        }
        return;
    }

    if (s_touch_active == 0U)
    {
        return;
    }

    dx = lcd_abs_diff_u16_simple(s_touch_start_x, touch_state.x);
    dy = lcd_abs_diff_u16_simple(s_touch_start_y, touch_state.y);
    s_touch_active = 0U;

    if (dx <= LCD_TOUCH_TAP_THRESHOLD && dy <= LCD_TOUCH_TAP_THRESHOLD)
    {
        (void)lcd_try_select_page_tab(s_touch_start_x, s_touch_start_y);
        return;
    }

    if (dx >= LCD_TOUCH_SWIPE_THRESHOLD && dx > dy)
    {
        if (touch_state.x > s_touch_start_x)
        {
            lcd_change_page_relative(-1);
        }
        else
        {
            lcd_change_page_relative(1);
        }
    }
}

static uint8_t lcd_status_changed(const App_DeviceStatus_t *current, const App_DeviceStatus_t *previous)
{
    if ((current == NULL) || (previous == NULL))
    {
        return 1U;
    }

    if ((current->online != previous->online) ||
        (current->relay1 != previous->relay1) ||
        (current->relay2 != previous->relay2) ||
        (current->weather.rain_detected != previous->weather.rain_detected) ||
        (current->weather.wind_query_failures != previous->weather.wind_query_failures) ||
        (current->weather.wind_query_error_count != previous->weather.wind_query_error_count) ||
        (current->weather.cj702_online != previous->weather.cj702_online) ||
        (current->weather.wind_online != previous->weather.wind_online) ||
        (current->weather.rain_online != previous->weather.rain_online) ||
        (current->last_status_publish_ok != previous->last_status_publish_ok) ||
        (current->l610_at_ready != previous->l610_at_ready) ||
        (current->l610_sim_ready != previous->l610_sim_ready) ||
        (current->l610_creg != previous->l610_creg) ||
        (current->l610_cgreg != previous->l610_cgreg) ||
        (current->l610_cgatt != previous->l610_cgatt))
    {
        return 1U;
    }

    if ((lcd_abs_diff_f(current->weather.wind_speed, previous->weather.wind_speed) > 0.1f) ||
        (lcd_abs_diff_f(current->weather.wind_direction, previous->weather.wind_direction) > 3.0f) ||
        (lcd_abs_diff_f(current->weather.temperature, previous->weather.temperature) > 0.1f) ||
        (lcd_abs_diff_f(current->weather.humidity, previous->weather.humidity) > 0.5f) ||
        (lcd_abs_diff_f(current->weather.pm25, previous->weather.pm25) > 1.0f) ||
        (lcd_abs_diff_f(current->weather.pm10, previous->weather.pm10) > 1.0f) ||
        (lcd_abs_diff_f(current->weather.co2, previous->weather.co2) > 5.0f) ||
        (lcd_abs_diff_f(current->weather.tvoc, previous->weather.tvoc) > 0.01f) ||
        (lcd_abs_diff_f(current->weather.ch2o, previous->weather.ch2o) > 0.01f))
    {
        return 1U;
    }

    if ((lcd_abs_diff_u16(current->weather.wind_speed_raw, previous->weather.wind_speed_raw) > 8U) ||
        (lcd_abs_diff_u16(current->weather.wind_direction_raw, previous->weather.wind_direction_raw) > 8U) ||
        (lcd_abs_diff_u16(current->weather.wind_query_raw, previous->weather.wind_query_raw) > 0U) ||
        (lcd_abs_diff_u16(current->weather.rain_adc_raw, previous->weather.rain_adc_raw) > 8U))
    {
        return 1U;
    }

    if (current->weather.wind_query_rx_count != previous->weather.wind_query_rx_count)
    {
        return 1U;
    }

    if (lcd_text_changed(current->pad.left_state, previous->pad.left_state) != 0U ||
        lcd_text_changed(current->pad.right_state, previous->pad.right_state) != 0U ||
        lcd_text_changed(current->pad.mode, previous->pad.mode) != 0U ||
        lcd_text_changed(current->state_text, previous->state_text) != 0U ||
        lcd_text_changed(current->last_error_text, previous->last_error_text) != 0U ||
        lcd_text_changed(current->last_cj702_frame_hex, previous->last_cj702_frame_hex) != 0U ||
        lcd_text_changed(current->l610_last_cmd, previous->l610_last_cmd) != 0U ||
        lcd_text_changed(current->l610_last_resp, previous->l610_last_resp) != 0U ||
        lcd_text_changed(current->l610_mqtt_stage, previous->l610_mqtt_stage) != 0U ||
        lcd_text_changed(current->weather.wind_query_target, previous->weather.wind_query_target) != 0U ||
        lcd_text_changed(current->weather.wind_last_tx_hex, previous->weather.wind_last_tx_hex) != 0U ||
        lcd_text_changed(current->weather.wind_last_rx_hex, previous->weather.wind_last_rx_hex) != 0U ||
        lcd_text_changed(current->weather.wind_direction_text, previous->weather.wind_direction_text) != 0U ||
        lcd_text_changed(current->weather.rain_level_text, previous->weather.rain_level_text) != 0U)
    {
        return 1U;
    }

    return 0U;
}

void LCD_DebugUI_Init(void)
{
    LCD_Port_Init();
    LCD_Touch_Init();
    LCD_Port_SetBacklight(100U);
    lcd_diag_print("[LCD] init ok\r\n");
    s_page = LCD_PAGE_OVERVIEW;
    s_last_refresh_tick = 0U;
    s_last_render_tick = 0U;
    s_refresh_count = 0U;
    s_force_render = 1U;
    s_has_last_status = 0U;
}

void LCD_DebugUI_Task(void)
{
    uint8_t key_pressed = Key_Scan();

    lcd_handle_touch_navigation();

    if (key_pressed != 0U)
    {
        lcd_change_page_relative(1);
    }

    App_FillDeviceStatus(&s_work_status);
    s_work_status.lcd_refresh_tick = s_last_render_tick;
    s_work_status.lcd_refresh_count = s_refresh_count;

    if ((s_force_render == 0U) &&
        (s_has_last_status != 0U) &&
        (lcd_status_changed(&s_work_status, &s_last_status) == 0U))
    {
        return;
    }

    if ((s_force_render == 0U) && ((HAL_GetTick() - s_last_refresh_tick) < 800U))
    {
        return;
    }

    s_last_refresh_tick = HAL_GetTick();
    s_last_render_tick = s_last_refresh_tick;
    s_refresh_count++;
    s_work_status.lcd_refresh_tick = s_last_render_tick;
    s_work_status.lcd_refresh_count = s_refresh_count;
    LCD_UI_ModelUpdate(&s_model, &s_work_status);
    LCD_UI_RenderPage(s_page, &s_model);
    memcpy(&s_last_status, &s_work_status, sizeof(s_last_status));
    s_has_last_status = 1U;
    s_force_render = 0U;
}

uint32_t LCD_DebugUI_GetRefreshTick(void)
{
    return s_last_render_tick;
}

uint32_t LCD_DebugUI_GetRefreshCount(void)
{
    return s_refresh_count;
}
