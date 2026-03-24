#include "lcd_ui_pages.h"
#include "lcd_port.h"
#include <stdio.h>

static const char *g_page_labels[LCD_PAGE_COUNT] = {
    "OVERVIEW",
    "PAD",
    "WEATHER",
    "DEBUG"
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
        LCD_Port_DrawText((uint16_t)(x + 14U), (uint16_t)(LCD_UI_TAB_Y + 11U), g_page_labels[index], text, 0U);
        x = (uint16_t)(x + LCD_UI_TAB_W + LCD_UI_TAB_GAP);
    }
}

static void draw_header(const char *title, LCD_Page_t page)
{
    char page_text[32];

    LCD_Port_FillRect(0, 0, LCD_UI_SCREEN_WIDTH, LCD_UI_HEADER_HEIGHT, LCD_PORT_COLOR_TEXT);
    LCD_Port_DrawText(18, 16, title, LCD_PORT_COLOR_CARD, 1U);
    snprintf(page_text, sizeof(page_text), "PAGE %u/%u", (unsigned)(page + 1U), (unsigned)LCD_PAGE_COUNT);
    LCD_Port_DrawText(650, 18, page_text, LCD_PORT_COLOR_CARD, 0U);
    draw_tabs(page);
}

static void draw_card(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *title, const char *value, uint16_t accent)
{
    LCD_Port_FillRect(x, y, w, h, LCD_PORT_COLOR_CARD);
    LCD_Port_DrawRect(x, y, w, h, accent);
    LCD_Port_DrawText((uint16_t)(x + 12U), (uint16_t)(y + 12U), title, LCD_PORT_COLOR_MUTED, 0U);
    LCD_Port_DrawText((uint16_t)(x + 12U), (uint16_t)(y + 40U), value, LCD_PORT_COLOR_TEXT, 1U);
}

static void render_overview(const LCD_UI_Model_t *model)
{
    char line[96];
    const App_DeviceStatus_t *s = &model->status;

    draw_header("DEBUG OVERVIEW", LCD_PAGE_OVERVIEW);

    snprintf(line, sizeof(line), "%s  %s", s->device_id, s->online ? "ONLINE" : "OFFLINE");
    draw_card(16, 76, 250, 100, "DEVICE", line, s->online ? LCD_PORT_COLOR_SUCCESS : LCD_PORT_COLOR_DANGER);

    snprintf(line, sizeof(line), "MQTT %u  RSSI %d", s->online, s->net.rssi);
    draw_card(276, 76, 250, 100, "NETWORK", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "%s / %s", s->pad.left_state, s->pad.right_state);
    draw_card(536, 76, 248, 100, "PAD", line, LCD_PORT_COLOR_WARN);

    snprintf(line, sizeof(line), "%.1fC  %.0f%%", (double)s->weather.temperature, (double)s->weather.humidity);
    draw_card(16, 190, 188, 100, "TEMP/HUM", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "%.1fm/s  %s %.1fdeg",
             (double)s->weather.wind_speed,
             s->weather.wind_direction_text,
             (double)s->weather.wind_direction);
    draw_card(212, 190, 188, 100, "WIND", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "PM2.5 %.0f", (double)s->weather.pm25);
    draw_card(408, 190, 188, 100, "AIR", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "%.0f%%", (double)s->weather.rain_value);
    draw_card(604, 190, 180, 100, "RAIN", line, LCD_PORT_COLOR_ACCENT);

    LCD_Port_FillRect(0, 316, LCD_UI_SCREEN_WIDTH, 28, LCD_PORT_COLOR_BG);
    LCD_Port_DrawText(18, 320, "Touch top tabs or swipe left/right to change pages.", LCD_PORT_COLOR_MUTED, 0U);
}

static void render_pad(const LCD_UI_Model_t *model)
{
    char line[96];
    const App_DeviceStatus_t *s = &model->status;

    draw_header("PAD STATUS", LCD_PAGE_PAD);
    snprintf(line, sizeof(line), "LEFT %s", s->pad.left_state);
    draw_card(16, 76, 240, 100, "LEFT DOOR", line, LCD_PORT_COLOR_ACCENT);
    snprintf(line, sizeof(line), "RIGHT %s", s->pad.right_state);
    draw_card(272, 76, 240, 100, "RIGHT DOOR", line, LCD_PORT_COLOR_ACCENT);
    snprintf(line, sizeof(line), "MODE %s", s->pad.mode);
    draw_card(528, 76, 256, 100, "MODE", line, LCD_PORT_COLOR_WARN);

    snprintf(line, sizeof(line), "READY %u  OCCUPIED %u", (unsigned)s->pad.ready, (unsigned)s->pad.occupied);
    draw_card(16, 190, 380, 100, "MISSION", line, LCD_PORT_COLOR_SUCCESS);

    snprintf(line, sizeof(line), "R1 %u  R2 %u", (unsigned)s->relay1, (unsigned)s->relay2);
    draw_card(408, 190, 376, 100, "RELAY", line, LCD_PORT_COLOR_ACCENT);

    LCD_Port_FillRect(0, 316, LCD_UI_SCREEN_WIDTH, 28, LCD_PORT_COLOR_BG);
    LCD_Port_DrawText(18, 320, s->state_text, LCD_PORT_COLOR_TEXT, 0U);
}

static void render_weather(const LCD_UI_Model_t *model)
{
    char line[96];
    const App_DeviceStatus_t *s = &model->status;

    draw_header("WEATHER", LCD_PAGE_WEATHER);

    snprintf(line, sizeof(line), "WIND %.1f m/s", (double)s->weather.wind_speed);
    draw_card(16, 76, 188, 92, "WIND SPEED", line, LCD_PORT_COLOR_ACCENT);
    snprintf(line, sizeof(line), "%s %.1f deg", s->weather.wind_direction_text, (double)s->weather.wind_direction);
    draw_card(212, 76, 188, 92, "WIND DIR", line, LCD_PORT_COLOR_ACCENT);
    snprintf(line, sizeof(line), "%.1f C", (double)s->weather.temperature);
    draw_card(408, 76, 188, 92, "TEMP", line, LCD_PORT_COLOR_ACCENT);
    snprintf(line, sizeof(line), "%.0f %%", (double)s->weather.humidity);
    draw_card(604, 76, 180, 92, "HUMIDITY", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "PM2.5 %.0f", (double)s->weather.pm25);
    draw_card(16, 184, 188, 92, "PM2.5", line, LCD_PORT_COLOR_ACCENT);
    snprintf(line, sizeof(line), "PM10 %.0f", (double)s->weather.pm10);
    draw_card(212, 184, 188, 92, "PM10", line, LCD_PORT_COLOR_ACCENT);
    snprintf(line, sizeof(line), "CO2 %.0f ppm", (double)s->weather.co2);
    draw_card(408, 184, 188, 92, "CO2", line, LCD_PORT_COLOR_ACCENT);
    snprintf(line, sizeof(line), "%.0f%%", (double)s->weather.rain_value);
    draw_card(604, 184, 180, 92, "RAIN", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "TVOC %.3f  CH2O %.3f", (double)s->weather.tvoc, (double)s->weather.ch2o);
    draw_card(16, 292, 380, 92, "GAS", line, LCD_PORT_COLOR_ACCENT);
    snprintf(line, sizeof(line), "WS %u  WD %u  R %u",
             (unsigned)s->weather.wind_speed_raw,
             (unsigned)s->weather.wind_direction_raw,
             (unsigned)s->weather.rain_adc_raw);
    draw_card(408, 292, 376, 92, "RAW INPUT", line, LCD_PORT_COLOR_WARN);
    LCD_Port_FillRect(0, 396, LCD_UI_SCREEN_WIDTH, 28, LCD_PORT_COLOR_BG);
    snprintf(line, sizeof(line), "RAIN %.3fV  TXT %s",
             (double)s->weather.rain_voltage,
             s->weather.rain_level_text);
    LCD_Port_DrawText(18, 400, line, LCD_PORT_COLOR_SUCCESS, 0U);
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
    draw_card(16, 76, 240, 88, "LCD", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "MQTT %s %s",
             s->l610_mqtt_stage,
             s->l610_mqtt_detail[0] ? s->l610_mqtt_detail : "");
    draw_card(272, 76, 240, 88, "MQTT STATE", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "AT %u SIM %u CREG %u ATT %u",
             (unsigned)s->l610_at_ready,
             (unsigned)s->l610_sim_ready,
             (unsigned)s->l610_creg,
             (unsigned)s->l610_cgatt);
    draw_card(528, 76, 256, 88, "L610", line, LCD_PORT_COLOR_WARN);

    draw_card(16, 178, 380, 88, "LAST CMD", s->l610_last_cmd[0] ? s->l610_last_cmd : "N/A", LCD_PORT_COLOR_ACCENT);
    draw_card(408, 178, 376, 88, "LAST RESP", s->l610_last_resp[0] ? s->l610_last_resp : "N/A", LCD_PORT_COLOR_SUCCESS);

    draw_card(16, 280, 376, 72, "WIND TX", s->weather.wind_last_tx_hex[0] ? s->weather.wind_last_tx_hex : "N/A", LCD_PORT_COLOR_ACCENT);
    draw_card(408, 280, 376, 72, "WIND RX", s->weather.wind_last_rx_hex[0] ? s->weather.wind_last_rx_hex : "N/A", LCD_PORT_COLOR_WARN);
    LCD_Port_FillRect(0, 366, LCD_UI_SCREEN_WIDTH, 28, LCD_PORT_COLOR_BG);
    snprintf(footer, sizeof(footer), "IP %s  %s %s R%u F%u RX%lu E%lu",
             s->net.ip[0] ? s->net.ip : "N/A",
             s->weather.adc_mode_text,
             s->weather.wind_query_target,
             (unsigned)s->weather.wind_query_raw,
             (unsigned)s->weather.wind_query_failures,
             (unsigned long)s->weather.wind_query_rx_count,
             (unsigned long)s->weather.wind_query_error_count);
    LCD_Port_DrawText(18, 370, footer, LCD_PORT_COLOR_DANGER, 0U);
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
    case LCD_PAGE_PAD:
        render_pad(model);
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
