#include "app_weather.h"
#include "bsp_uart.h"
#include "sensor_wind_zts.h"
#include "sensor_air_cj702.h"
#include "sensor_rain.h"
#include "adc.h"
#include "weather_data.h"
#include "stm32h7xx_hal.h"
#include <string.h>

#define APP_RAIN_REFRESH_PERIOD_MS    200U
#define APP_WIND_REFRESH_PERIOD_MS    20U
#define APP_AIR_REFRESH_PERIOD_MS     200U
#define APP_WEATHER_MODE_TEXT         "W485+RADC"
#define APP_RAIN_ERROR_TEXT           "RADC_ERR"

static uint32_t tick_rain_refresh = 0U;
static uint32_t tick_wind_refresh = 0U;
static uint32_t tick_air_refresh = 0U;

static void AppWeather_SetModeText(const char *text)
{
    if (text == NULL)
    {
        text = APP_WEATHER_MODE_TEXT;
    }

    strncpy(g_weather_data.adc_mode_text, text, sizeof(g_weather_data.adc_mode_text) - 1U);
    g_weather_data.adc_mode_text[sizeof(g_weather_data.adc_mode_text) - 1U] = '\0';
}

static HAL_StatusTypeDef AppWeather_RefreshRainInputs(void)
{
    if (WeatherADC_RefreshFallback() != HAL_OK)
    {
        return HAL_ERROR;
    }

    RainSensor_Poll();
    return HAL_OK;
}

void AppWeather_Init(void)
{
    uint32_t now = HAL_GetTick();

    BSP_Uart_Init();
    WindSensor_Init();
    AirSensor_Init();
    RainSensor_Init();

    g_weather_data.wind_speed_mps = 0.0f;
    g_weather_data.wind_dir_deg = 0.0f;
    g_weather_data.wind_speed_raw = 0U;
    g_weather_data.wind_direction_raw = 0U;
    g_weather_data.wind_speed_voltage = 0.0f;
    g_weather_data.wind_direction_voltage = 0.0f;
    g_weather_data.temperature_c = 0.0f;
    g_weather_data.humidity_rh = 0.0f;
    g_weather_data.co2_ppm = 0.0f;
    g_weather_data.pm25_ugm3 = 0.0f;
    g_weather_data.pm10_ugm3 = 0.0f;
    g_weather_data.tvoc_mg_m3 = 0.0f;
    g_weather_data.ch2o_mg_m3 = 0.0f;
    g_weather_data.rain_adc_raw = 0U;
    g_weather_data.rain_voltage = 0.0f;
    g_weather_data.rain_detected = 0U;
    g_weather_data.rain_value = 0.0f;
    g_weather_data.wind_online = 0U;
    g_weather_data.air_online = 0U;
    g_weather_data.rain_online = 0U;

    strncpy(g_weather_data.wind_direction_text, "N/A", sizeof(g_weather_data.wind_direction_text) - 1U);
    g_weather_data.wind_direction_text[sizeof(g_weather_data.wind_direction_text) - 1U] = '\0';
    strncpy(g_weather_data.rain_level_text, "0%", sizeof(g_weather_data.rain_level_text) - 1U);
    g_weather_data.rain_level_text[sizeof(g_weather_data.rain_level_text) - 1U] = '\0';
    AppWeather_SetModeText(APP_WEATHER_MODE_TEXT);

    tick_rain_refresh = now - APP_RAIN_REFRESH_PERIOD_MS;
    tick_wind_refresh = now - APP_WIND_REFRESH_PERIOD_MS;
    tick_air_refresh = now - APP_AIR_REFRESH_PERIOD_MS;
}

void AppWeather_Task(void)
{
    uint32_t now = HAL_GetTick();

    if ((now - tick_rain_refresh) >= APP_RAIN_REFRESH_PERIOD_MS)
    {
        tick_rain_refresh = now;
        if (AppWeather_RefreshRainInputs() != HAL_OK)
        {
            RainSensor_MarkOffline();
            AppWeather_SetModeText(APP_RAIN_ERROR_TEXT);
        }
        else
        {
            AppWeather_SetModeText(APP_WEATHER_MODE_TEXT);
        }
    }

    if ((now - tick_wind_refresh) >= APP_WIND_REFRESH_PERIOD_MS)
    {
        tick_wind_refresh = now;
        WindSensor_Poll();
    }

    if ((now - tick_air_refresh) >= APP_AIR_REFRESH_PERIOD_MS)
    {
        tick_air_refresh = now;
        AirSensor_Poll();
    }

    WindSensor_Parse();
    AirSensor_Parse();
    RainSensor_Parse();
}
