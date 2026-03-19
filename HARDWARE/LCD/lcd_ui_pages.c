#include "lcd_ui_pages.h"
#include "lcd_port.h"
#include <stdio.h>

static const char *g_page_labels[LCD_PAGE_COUNT] = {
    "OVERVIEW",
    "PAD",
    "WEATHER",
    "DEBUG"
};

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

    snprintf(line, sizeof(line), "%.1fm/s  %.0fdeg", (double)s->weather.wind_speed, (double)s->weather.wind_direction);
    draw_card(212, 190, 188, 100, "WIND", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "PM2.5 %.0f", (double)s->weather.pm25);
    draw_card(408, 190, 188, 100, "AIR", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "%s", s->weather.rain_detected ? "RAIN DETECTED" : "NO RAIN");
    draw_card(604, 190, 180, 100, "RAIN", line, LCD_PORT_COLOR_ACCENT);

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

    LCD_Port_DrawText(18, 320, s->state_text, LCD_PORT_COLOR_TEXT, 0U);
}

static void render_weather(const LCD_UI_Model_t *model)
{
    char line[96];
    const App_DeviceStatus_t *s = &model->status;

    draw_header("WEATHER", LCD_PAGE_WEATHER);

    snprintf(line, sizeof(line), "WIND %.1f m/s", (double)s->weather.wind_speed);
    draw_card(16, 76, 188, 92, "WIND SPEED", line, LCD_PORT_COLOR_ACCENT);
    snprintf(line, sizeof(line), "%.0f deg", (double)s->weather.wind_direction);
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
    snprintf(line, sizeof(line), "%s", s->weather.rain_detected ? "RAIN" : "CLEAR");
    draw_card(604, 184, 180, 92, "RAIN", line, LCD_PORT_COLOR_ACCENT);

    snprintf(line, sizeof(line), "TVOC %.3f  CH2O %.3f", (double)s->weather.tvoc, (double)s->weather.ch2o);
    draw_card(16, 292, 380, 92, "GAS", line, LCD_PORT_COLOR_ACCENT);
    snprintf(line, sizeof(line), "ADC %u / %u", (unsigned)s->weather.wind_adc_raw, (unsigned)s->weather.direction_adc_raw);
    draw_card(408, 292, 376, 92, "RAW ADC", line, LCD_PORT_COLOR_WARN);
}

static void render_debug(const LCD_UI_Model_t *model)
{
    char line[128];
    const App_DeviceStatus_t *s = &model->status;

    draw_header("DEBUG", LCD_PAGE_DEBUG);
    snprintf(line, sizeof(line), "PUBLISH OK %u  TICK %lu", (unsigned)s->last_status_publish_ok, (unsigned long)s->last_status_publish_tick);
    draw_card(16, 76, 380, 100, "MQTT", line, LCD_PORT_COLOR_ACCENT);
    snprintf(line, sizeof(line), "AIR %u  WIND %u  RAIN %u", (unsigned)s->weather.cj702_online, (unsigned)s->weather.wind_online, (unsigned)s->weather.rain_online);
    draw_card(408, 76, 376, 100, "SENSOR ONLINE", line, LCD_PORT_COLOR_ACCENT);
    draw_card(16, 190, 768, 88, "LAST CJ702 FRAME", s->last_cj702_frame_hex[0] ? s->last_cj702_frame_hex : "N/A", LCD_PORT_COLOR_WARN);
    draw_card(16, 292, 768, 88, "LAST ERROR", s->last_error_text[0] ? s->last_error_text : "NONE", LCD_PORT_COLOR_DANGER);
}

void LCD_UI_RenderPage(LCD_Page_t page, const LCD_UI_Model_t *model)
{
    if (model == 0)
    {
        return;
    }

    LCD_Port_BeginFrame();
    LCD_Port_Clear(LCD_PORT_COLOR_BG);

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
