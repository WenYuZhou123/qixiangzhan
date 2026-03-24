#include "sensor_rain.h"
#include "adc.h"
#include "main.h"
#include "weather_data.h"
#include "stm32h7xx_hal.h"
#include <stdio.h>
#include <string.h>

#define RAIN_RAW_INDEX                2U
#define RAIN_ADC_MAX_COUNTS           65535.0f
#define RAIN_ADC_REF_VOLTAGE          3.3f
#define RAIN_FILTER_OLD_WEIGHT        0.75f
#define RAIN_FILTER_NEW_WEIGHT        0.25f
#define RAIN_WETNESS_MAX_PERCENT      100.0f
#define RAIN_DETECTED_SET_THRESHOLD   8.0f
#define RAIN_DETECTED_CLEAR_THRESHOLD 4.0f
#define RAIN_DRY_TRACK_OLD_WEIGHT     0.98f
#define RAIN_DRY_TRACK_NEW_WEIGHT     0.02f

RainSensor_t g_rain_sensor = {0};
static float s_rain_filtered_raw = 0.0f;
static uint8_t s_rain_filter_ready = 0U;
static float s_rain_dry_reference_raw = RAIN_ADC_MAX_COUNTS;
static uint8_t s_rain_dry_reference_ready = 0U;

static float RainSensor_AdcToVoltage(uint16_t raw)
{
    return ((float)raw * RAIN_ADC_REF_VOLTAGE) / RAIN_ADC_MAX_COUNTS;
}

static void RainSensor_UpdateDryReference(uint16_t raw, uint8_t detected_state)
{
    float next_raw = (float)raw;

    if (s_rain_dry_reference_ready == 0U)
    {
        s_rain_dry_reference_raw = next_raw;
        s_rain_dry_reference_ready = 1U;
        return;
    }

    if (detected_state == 0U)
    {
        s_rain_dry_reference_raw = (s_rain_dry_reference_raw * RAIN_DRY_TRACK_OLD_WEIGHT) +
                                   (next_raw * RAIN_DRY_TRACK_NEW_WEIGHT);
    }

    if (s_rain_dry_reference_raw < 1.0f)
    {
        s_rain_dry_reference_raw = 1.0f;
    }
    if (s_rain_dry_reference_raw > RAIN_ADC_MAX_COUNTS)
    {
        s_rain_dry_reference_raw = RAIN_ADC_MAX_COUNTS;
    }
}

static float RainSensor_AdcToWetnessPercent(uint16_t raw)
{
    float wetness_percent;

    if (s_rain_dry_reference_ready == 0U || s_rain_dry_reference_raw <= 1.0f)
    {
        return 0.0f;
    }

    wetness_percent = ((s_rain_dry_reference_raw - (float)raw) / s_rain_dry_reference_raw) * 100.0f;
    if (wetness_percent < 0.0f)
    {
        wetness_percent = 0.0f;
    }
    if (wetness_percent > RAIN_WETNESS_MAX_PERCENT)
    {
        wetness_percent = RAIN_WETNESS_MAX_PERCENT;
    }

    return wetness_percent;
}

static void RainSensor_FormatText(float wetness_percent, char *text_out, uint32_t text_size)
{
    if (text_out == NULL || text_size == 0U)
    {
        return;
    }

    (void)snprintf(text_out, text_size, "%.0f%%", (double)wetness_percent);
    text_out[text_size - 1U] = '\0';
}

static uint16_t RainSensor_FilterRaw(uint16_t raw_sample)
{
    float next_raw = (float)raw_sample;

    if (s_rain_filter_ready == 0U)
    {
        s_rain_filtered_raw = next_raw;
        s_rain_filter_ready = 1U;
    }
    else
    {
        s_rain_filtered_raw = (s_rain_filtered_raw * RAIN_FILTER_OLD_WEIGHT) +
                              (next_raw * RAIN_FILTER_NEW_WEIGHT);
    }

    if (s_rain_filtered_raw < 0.0f)
    {
        s_rain_filtered_raw = 0.0f;
    }
    if (s_rain_filtered_raw > RAIN_ADC_MAX_COUNTS)
    {
        s_rain_filtered_raw = RAIN_ADC_MAX_COUNTS;
    }

    return (uint16_t)(s_rain_filtered_raw + 0.5f);
}

static uint8_t RainSensor_UpdateDetected(float rain_mm, uint8_t current_state)
{
    if (current_state != 0U)
    {
        return (uint8_t)((rain_mm <= RAIN_DETECTED_CLEAR_THRESHOLD) ? 0U : 1U);
    }

    return (uint8_t)((rain_mm >= RAIN_DETECTED_SET_THRESHOLD) ? 1U : 0U);
}

void RainSensor_Init(void)
{
    s_rain_filtered_raw = 0.0f;
    s_rain_filter_ready = 0U;
    s_rain_dry_reference_raw = RAIN_ADC_MAX_COUNTS;
    s_rain_dry_reference_ready = 0U;
    g_rain_sensor.base.online = 0U;
    g_rain_sensor.base.last_update_tick = 0U;
    g_rain_sensor.rain_adc_raw = 0U;
    g_rain_sensor.rain_value = 0.0f;
    g_rain_sensor.rain_detected = 0U;
    RainSensor_FormatText(0.0f, g_rain_sensor.rain_level_text, sizeof(g_rain_sensor.rain_level_text));
    g_weather_data.rain_adc_raw = 0U;
    g_weather_data.rain_voltage = 0.0f;
    g_weather_data.rain_value = 0.0f;
    g_weather_data.rain_detected = 0U;
    RainSensor_FormatText(0.0f, g_weather_data.rain_level_text, sizeof(g_weather_data.rain_level_text));
    g_weather_data.rain_online = 0U;
}

void RainSensor_MarkOffline(void)
{
    s_rain_filtered_raw = 0.0f;
    s_rain_filter_ready = 0U;
    s_rain_dry_reference_raw = RAIN_ADC_MAX_COUNTS;
    s_rain_dry_reference_ready = 0U;
    g_rain_sensor.base.online = 0U;
    g_rain_sensor.base.last_update_tick = 0U;
    g_rain_sensor.rain_adc_raw = 0U;
    g_rain_sensor.rain_value = 0.0f;
    g_rain_sensor.rain_detected = 0U;
    RainSensor_FormatText(0.0f, g_rain_sensor.rain_level_text, sizeof(g_rain_sensor.rain_level_text));

    g_weather_data.rain_adc_raw = 0U;
    g_weather_data.rain_voltage = 0.0f;
    g_weather_data.rain_value = 0.0f;
    g_weather_data.rain_detected = 0U;
    RainSensor_FormatText(0.0f, g_weather_data.rain_level_text, sizeof(g_weather_data.rain_level_text));
    g_weather_data.rain_online = 0U;
}

void RainSensor_Poll(void)
{
    uint16_t raw = RainSensor_FilterRaw(WeatherADC_ReadRaw(RAIN_RAW_INDEX));
    float wetness_percent;

    RainSensor_UpdateDryReference(raw, g_rain_sensor.rain_detected);
    wetness_percent = RainSensor_AdcToWetnessPercent(raw);

    g_rain_sensor.rain_adc_raw = raw;
    g_rain_sensor.rain_value = wetness_percent;
    g_rain_sensor.rain_detected = RainSensor_UpdateDetected(wetness_percent, g_rain_sensor.rain_detected);
    RainSensor_FormatText(wetness_percent, g_rain_sensor.rain_level_text, sizeof(g_rain_sensor.rain_level_text));
    g_rain_sensor.base.online = 1U;
    g_rain_sensor.base.last_update_tick = HAL_GetTick();

    g_weather_data.rain_adc_raw = raw;
    g_weather_data.rain_voltage = RainSensor_AdcToVoltage(raw);
    g_weather_data.rain_detected = g_rain_sensor.rain_detected;
    g_weather_data.rain_value = g_rain_sensor.rain_value;
    strncpy(g_weather_data.rain_level_text,
            g_rain_sensor.rain_level_text,
            sizeof(g_weather_data.rain_level_text) - 1U);
    g_weather_data.rain_level_text[sizeof(g_weather_data.rain_level_text) - 1U] = '\0';
    g_weather_data.rain_online = 1U;
}

void RainSensor_Parse(void)
{
    /* Analog rain sensor uses ADC polling only. */
}
