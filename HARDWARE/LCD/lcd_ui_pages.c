#include "lcd_ui_pages.h"

#include "lcd_port.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    LCD_ControlAction_t action;
    const char *label;
    uint16_t fill;
} LCD_ControlButtonDef_t;

static const char *g_page_labels[LCD_PAGE_COUNT] = {
    "OVERVIEW",
    "CONTROL",
    "WEATHER",
    "DEBUG"
};

static const LCD_ControlButtonDef_t g_control_buttons[] = {
    {16U, 286U, 180U, 54U, LCD_CONTROL_ACTION_R1_ON,         "R1 ON",   LCD_PORT_COLOR_SUCCESS},
    {208U, 286U, 180U, 54U, LCD_CONTROL_ACTION_R1_OFF,        "R1 OFF",  LCD_PORT_COLOR_DANGER},
    {400U, 286U, 180U, 54U, LCD_CONTROL_ACTION_R2_ON,         "R2 ON",   LCD_PORT_COLOR_SUCCESS},
    {592U, 286U, 180U, 54U, LCD_CONTROL_ACTION_R2_OFF,        "R2 OFF",  LCD_PORT_COLOR_DANGER},
    {16U, 352U, 180U, 54U, LCD_CONTROL_ACTION_ALL_ON,         "ALL ON",  LCD_PORT_COLOR_SUCCESS},
    {208U, 352U, 180U, 54U, LCD_CONTROL_ACTION_ALL_OFF,       "ALL OFF", LCD_PORT_COLOR_DANGER},
    {400U, 352U, 180U, 54U, LCD_CONTROL_ACTION_QUERY_STATUS,  "REFRESH", LCD_PORT_COLOR_ACCENT}
};

static LCD_Page_t g_last_page = LCD_PAGE_COUNT;

static void draw_tabs(LCD_Page_t page)
{
    uint16_t x = LCD_UI_TAB_X;
    uint8_t index;

    for (index = 0U; index < LCD_PAGE_COUNT; index++)
    {
        uint16_t fill = (index == (uint8_t)page) ? LCD_PORT_COLOR_ACCENT : LCD_PORT_COLOR_BG;
        uint16_t text = (index == (uint8_t)page) ? LCD_PORT_COLOR_CARD : LCD_PORT_COLOR_MUTED;

        LCD_Port_FillRect(x, LCD_UI_TAB_Y, LCD_UI_TAB_W, LCD_UI_TAB_H, fill);
        LCD_Port_DrawRect(x, LCD_UI_TAB_Y, LCD_UI_TAB_W, LCD_UI_TAB_H, LCD_PORT_COLOR_CARD);
        LCD_Port_DrawText((uint16_t)(x + 10U), (uint16_t)(LCD_UI_TAB_Y + 11U), g_page_labels[index], text, 0U);
        x = (uint16_t)(x + LCD_UI_TAB_W + LCD_UI_TAB_GAP);
    }
}

static void draw_header(const char *title, LCD_Page_t page)
{
    char page_text[32];

    LCD_Port_FillRect(0U, 0U, LCD_UI_SCREEN_WIDTH, LCD_UI_HEADER_HEIGHT, LCD_PORT_COLOR_TEXT);
    LCD_Port_DrawText(18U, 16U, title, LCD_PORT_COLOR_CARD, 1U);
    snprintf(page_text, sizeof(page_text), "PAGE %u/%u", (unsigned)(page + 1U), (unsigned)LCD_PAGE_COUNT);
    LCD_Port_DrawText(650U, 18U, page_text, LCD_PORT_COLOR_CARD, 0U);
    draw_tabs(page);
}

static void draw_card(uint16_t x,
                      uint16_t y,
                      uint16_t w,
                      uint16_t h,
                      const char *title,
                      const char *value,
                      uint16_t accent)
{
    LCD_Port_FillRect(x, y, w, h, LCD_PORT_COLOR_CARD);
    LCD_Port_DrawRect(x, y, w, h, accent);
    LCD_Port_DrawText((uint16_t)(x + 12U), (uint16_t)(y + 12U), title, LCD_PORT_COLOR_MUTED, 0U);
    LCD_Port_DrawText((uint16_t)(x + 12U), (uint16_t)(y + 40U), value, LCD_PORT_COLOR_TEXT, 1U);
}

static void draw_button(const LCD_ControlButtonDef_t *button, uint8_t enabled)
{
    uint16_t fill = enabled != 0U ? button->fill : LCD_PORT_COLOR_MUTED;
    uint16_t text = enabled != 0U ? LCD_PORT_COLOR_CARD : LCD_PORT_COLOR_TEXT;

    LCD_Port_FillRect(button->x, button->y, button->w, button->h, fill);
    LCD_Port_DrawRect(button->x, button->y, button->w, button->h, LCD_PORT_COLOR_CARD);
    LCD_Port_DrawText((uint16_t)(button->x + 44U), (uint16_t)(button->y + 18U), button->label, text, 0U);
}

static void render_overview(const LCD_UI_Model_t *model)
{
    char line[128];
    const App_DeviceStatus_t *s = &model->status;

    draw_header("MISSION OVERVIEW", LCD_PAGE_OVERVIEW);

    snprintf(line, sizeof(line), "%s  %s", s->device_id, s->online ? "ONLINE" : "OFFLINE");
    draw_card(16U, 76U, 250U, 100U, "DEVICE", line, s->online ? LCD_PORT_COLOR_SUCCESS : LCD_PORT_COLOR_DANGER);

    snprintf(line, sizeof(line), "MQTT %s  RSSI %d", s->online ? "LIVE" : "WAIT", s->net.rssi);
    draw_card(276U, 76U, 250U, 100U, "LINK", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "%s / %s", s->pad.left_state, s->pad.right_state);
    draw_card(536U, 76U, 248U, 100U, "PAD", line, LCD_PORT_COLOR_WARN);

    snprintf(line, sizeof(line), "%.1fC  %.0f%%", (double)s->weather.temperature, (double)s->weather.humidity);
    draw_card(16U, 190U, 188U, 100U, "TEMP/HUM", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "%.1fm/s  %s %.0fdeg",
             (double)s->weather.wind_speed,
             s->weather.wind_direction_text,
             (double)s->weather.wind_direction);
    draw_card(212U, 190U, 188U, 100U, "WIND", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "R1 %s  R2 %s", s->relay1 ? "ON" : "OFF", s->relay2 ? "ON" : "OFF");
    draw_card(408U, 190U, 188U, 100U, "CONTROL", line, LCD_PORT_COLOR_SUCCESS);

    snprintf(line, sizeof(line), "%s  %.0f%%", s->weather.rain_detected ? "WET" : "DRY", (double)s->weather.rain_value);
    draw_card(604U, 190U, 180U, 100U, "RAIN", line, LCD_PORT_COLOR_ACCENT);

    LCD_Port_FillRect(0U, 316U, LCD_UI_SCREEN_WIDTH, 28U, LCD_PORT_COLOR_BG);
    LCD_Port_DrawText(18U, 320U, "Measured weather only  Swipe left or right for more", LCD_PORT_COLOR_MUTED, 0U);
}

static void render_control(const LCD_UI_Model_t *model)
{
    char line[128];
    const App_DeviceStatus_t *s = &model->status;
    uint8_t index;
    uint8_t buttons_enabled = (uint8_t)((model->control_busy == 0U) ? 1U : 0U);

    draw_header("CONTROL CENTER", LCD_PAGE_CONTROL);

    snprintf(line, sizeof(line), "%s  %s", s->device_id, s->online ? "ONLINE" : "OFFLINE");
    draw_card(16U, 76U, 248U, 86U, "DEVICE", line, s->online ? LCD_PORT_COLOR_SUCCESS : LCD_PORT_COLOR_DANGER);

    snprintf(line, sizeof(line), "R1 %s  R2 %s",
             s->relay1 != 0U ? "ON" : "OFF",
             s->relay2 != 0U ? "ON" : "OFF");
    draw_card(276U, 76U, 248U, 86U, "RELAY", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "LEFT %s / RIGHT %s", s->pad.left_state, s->pad.right_state);
    draw_card(536U, 76U, 248U, 86U, "PAD", line, LCD_PORT_COLOR_WARN);

    snprintf(line, sizeof(line), "MODE %s  READY %u  OCC %u",
             s->pad.mode,
             (unsigned)s->pad.ready,
             (unsigned)s->pad.occupied);
    draw_card(16U, 174U, 376U, 90U, "PAD STATE", line, s->pad.ready ? LCD_PORT_COLOR_SUCCESS : LCD_PORT_COLOR_WARN);

    snprintf(line, sizeof(line), "ACK %s  WIND %.1f m/s  RAIN %s",
             s->last_status_publish_ok != 0U ? "OK" : "WAIT",
             (double)s->weather.wind_speed,
             s->weather.rain_detected ? "YES" : "NO");
    draw_card(408U, 174U, 376U, 90U, "STATE", line, LCD_PORT_COLOR_ACCENT);

    for (index = 0U; index < (uint8_t)(sizeof(g_control_buttons) / sizeof(g_control_buttons[0])); index++)
    {
        draw_button(&g_control_buttons[index], buttons_enabled);
    }

    LCD_Port_FillRect(592U, 352U, 180U, 54U, model->control_busy != 0U ? LCD_PORT_COLOR_WARN : LCD_PORT_COLOR_SUCCESS);
    LCD_Port_DrawRect(592U, 352U, 180U, 54U, LCD_PORT_COLOR_CARD);
    LCD_Port_DrawText(634U, 370U, model->control_busy != 0U ? "BUSY" : "READY", LCD_PORT_COLOR_CARD, 0U);

    LCD_Port_FillRect(0U, 418U, LCD_UI_SCREEN_WIDTH, 28U, LCD_PORT_COLOR_BG);
    LCD_Port_DrawText(18U,
                      422U,
                      model->control_message[0] != '\0' ? model->control_message : "Tap buttons to send relay or refresh commands.",
                      model->control_busy != 0U ? LCD_PORT_COLOR_WARN : LCD_PORT_COLOR_TEXT,
                      0U);
}

static void render_weather(const LCD_UI_Model_t *model)
{
    char line[128];
    const App_DeviceStatus_t *s = &model->status;

    draw_header("WEATHER", LCD_PAGE_WEATHER);

    snprintf(line, sizeof(line), "WIND %.1f m/s", (double)s->weather.wind_speed);
    draw_card(16U, 76U, 188U, 92U, "WIND SPEED", line, LCD_PORT_COLOR_ACCENT);
    snprintf(line, sizeof(line), "%s %.0f deg", s->weather.wind_direction_text, (double)s->weather.wind_direction);
    draw_card(212U, 76U, 188U, 92U, "WIND DIR", line, LCD_PORT_COLOR_ACCENT);
    snprintf(line, sizeof(line), "%.1f C", (double)s->weather.temperature);
    draw_card(408U, 76U, 188U, 92U, "TEMP", line, LCD_PORT_COLOR_ACCENT);
    snprintf(line, sizeof(line), "%.0f %%", (double)s->weather.humidity);
    draw_card(604U, 76U, 180U, 92U, "HUMIDITY", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "PM2.5 %.0f  PM10 %.0f", (double)s->weather.pm25, (double)s->weather.pm10);
    draw_card(16U, 184U, 384U, 92U, "PARTICLES", line, LCD_PORT_COLOR_ACCENT);
    snprintf(line, sizeof(line), "CO2 %.0f  TVOC %.3f", (double)s->weather.co2, (double)s->weather.tvoc);
    draw_card(408U, 184U, 376U, 92U, "AIR", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "%s  %.0f%%  CH2O %.3f",
             s->weather.rain_detected ? "RAIN YES" : "RAIN NO",
             (double)s->weather.rain_value,
             (double)s->weather.ch2o);
    draw_card(16U, 292U, 380U, 92U, "RAIN / GAS", line, LCD_PORT_COLOR_ACCENT);
    snprintf(line, sizeof(line), "WS %u  WD %u  R %u",
             (unsigned)s->weather.wind_speed_raw,
             (unsigned)s->weather.wind_direction_raw,
             (unsigned)s->weather.rain_adc_raw);
    draw_card(408U, 292U, 376U, 92U, "RAW INPUT", line, LCD_PORT_COLOR_WARN);

    LCD_Port_FillRect(0U, 396U, LCD_UI_SCREEN_WIDTH, 28U, LCD_PORT_COLOR_BG);
    snprintf(line, sizeof(line), "DIR %s  RAIN %.3fV  TXT %s",
             s->weather.wind_direction_text,
             (double)s->weather.rain_voltage,
             s->weather.rain_level_text);
    LCD_Port_DrawText(18U, 400U, line, LCD_PORT_COLOR_SUCCESS, 0U);
}

static void render_debug(const LCD_UI_Model_t *model)
{
    char line[128];
    char footer[128];
    const App_DeviceStatus_t *s = &model->status;

    draw_header("DEBUG", LCD_PAGE_DEBUG);
    snprintf(line, sizeof(line), "REF %lu  TICK %lu",
             (unsigned long)s->lcd_refresh_count,
             (unsigned long)s->lcd_refresh_tick);
    draw_card(16U, 76U, 240U, 88U, "LCD", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "MQTT %s %s",
             s->l610_mqtt_stage,
             s->l610_mqtt_detail[0] ? s->l610_mqtt_detail : "");
    draw_card(272U, 76U, 240U, 88U, "MQTT STATE", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "AT %u SIM %u CREG %u ATT %u",
             (unsigned)s->l610_at_ready,
             (unsigned)s->l610_sim_ready,
             (unsigned)s->l610_creg,
             (unsigned)s->l610_cgatt);
    draw_card(528U, 76U, 256U, 88U, "L610", line, LCD_PORT_COLOR_WARN);

    draw_card(16U, 178U, 380U, 88U, "LAST CMD", s->l610_last_cmd[0] ? s->l610_last_cmd : "N/A", LCD_PORT_COLOR_ACCENT);
    draw_card(408U, 178U, 376U, 88U, "LAST RESP", s->l610_last_resp[0] ? s->l610_last_resp : "N/A", LCD_PORT_COLOR_SUCCESS);

    draw_card(16U, 280U, 376U, 72U, "WIND TX", s->weather.wind_last_tx_hex[0] ? s->weather.wind_last_tx_hex : "N/A", LCD_PORT_COLOR_ACCENT);
    draw_card(408U, 280U, 376U, 72U, "WIND RX", s->weather.wind_last_rx_hex[0] ? s->weather.wind_last_rx_hex : "N/A", LCD_PORT_COLOR_WARN);

    LCD_Port_FillRect(0U, 366U, LCD_UI_SCREEN_WIDTH, 28U, LCD_PORT_COLOR_BG);
    snprintf(footer, sizeof(footer), "IP %s  %s %s R%u F%u RX%lu E%lu",
             s->net.ip[0] ? s->net.ip : "N/A",
             s->weather.adc_mode_text,
             s->weather.wind_query_target,
             (unsigned)s->weather.wind_query_raw,
             (unsigned)s->weather.wind_query_failures,
             (unsigned long)s->weather.wind_query_rx_count,
             (unsigned long)s->weather.wind_query_error_count);
    LCD_Port_DrawText(18U, 370U, footer, LCD_PORT_COLOR_DANGER, 0U);

    snprintf(footer, sizeof(footer), "SENS %u/%u/%u FAIL %u LAST %lu",
             (unsigned)s->weather.wind_online,
             (unsigned)s->weather.air_online,
             (unsigned)s->weather.rain_online,
             (unsigned)s->weather.sensor_failure_count,
             (unsigned long)s->weather.sensor_last_ok_tick);
    LCD_Port_DrawText(18U, 392U, footer, LCD_PORT_COLOR_SUCCESS, 0U);
}

void LCD_UI_RenderPage(LCD_Page_t page, const LCD_UI_Model_t *model)
{
    if (model == 0)
    {
        return;
    }

    LCD_Port_BeginFrame();
    if (page != g_last_page)
    {
        LCD_Port_Clear(LCD_PORT_COLOR_BG);
        g_last_page = page;
    }

    switch (page)
    {
    case LCD_PAGE_OVERVIEW:
        render_overview(model);
        break;
    case LCD_PAGE_CONTROL:
        render_control(model);
        break;
    case LCD_PAGE_WEATHER:
        render_weather(model);
        break;
    case LCD_PAGE_DEBUG:
    default:
        render_debug(model);
        break;
    }

    LCD_Port_EndFrame();
}

LCD_ControlAction_t LCD_UI_ControlHitTest(uint16_t x, uint16_t y)
{
    uint8_t index;

    for (index = 0U; index < (uint8_t)(sizeof(g_control_buttons) / sizeof(g_control_buttons[0])); index++)
    {
        const LCD_ControlButtonDef_t *button = &g_control_buttons[index];
        if ((x >= button->x) && (x < (uint16_t)(button->x + button->w)) &&
            (y >= button->y) && (y < (uint16_t)(button->y + button->h)))
        {
            return button->action;
        }
    }

    return LCD_CONTROL_ACTION_NONE;
}
