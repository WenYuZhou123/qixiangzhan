#include "sensor_wind_zts.h"
#include "adc.h"
#include "weather_data.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_adc.h"

/*
 * Current implementation assumes the wind-speed / wind-direction signal has already
 * been converted from the sensor's differential industrial analog output to a safe
 * single-ended ADC voltage referenced to MCU GND.
 *
 * Do NOT directly connect the raw differential output pair of a 10-30V transmitter
 * to STM32 ADC pins before confirming the frontend circuit and common-reference rule.
 */

#define WIND_SPEED_ADC_CHANNEL         ADC_CHANNEL_3
#define WIND_DIRECTION_ADC_CHANNEL     ADC_CHANNEL_4
#define WIND_ADC_MAX_COUNTS            65535.0f
#define WIND_ADC_REF_VOLTAGE           3.3f
#define WIND_SENSOR_OUTPUT_MAX_VOLTAGE 2.0f
#define WIND_SPEED_FULL_SCALE_MPS      30.0f

WindSensor_t g_wind_sensor = {0};

static uint16_t WindSensor_ReadAdcChannel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_64CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
    sConfig.OffsetSignedSaturation = DISABLE;

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        return 0U;
    }
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return 0U;
    }
    if (HAL_ADC_PollForConversion(&hadc1, 20U) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        return 0U;
    }

    {
        uint16_t value = (uint16_t)HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);
        return value;
    }
}

static float WindSensor_AdcToVoltage(uint16_t adc_raw)
{
    return ((float)adc_raw / WIND_ADC_MAX_COUNTS) * WIND_ADC_REF_VOLTAGE;
}

static float WindSensor_DecodeDirection(float voltage)
{
    static const float direction_voltages[8] = {
        0.00f, 0.25f, 0.50f, 0.75f, 1.00f, 1.25f, 1.50f, 1.75f
    };
    static const float direction_degrees[8] = {
        0.0f, 45.0f, 90.0f, 135.0f, 180.0f, 225.0f, 270.0f, 315.0f
    };

    uint32_t i = 0U;
    uint32_t best_index = 0U;
    float best_diff = 100.0f;

    for (i = 0U; i < 8U; ++i)
    {
        float diff = voltage - direction_voltages[i];
        if (diff < 0.0f)
        {
            diff = -diff;
        }
        if (diff < best_diff)
        {
            best_diff = diff;
            best_index = i;
        }
    }

    return direction_degrees[best_index];
}

void WindSensor_Init(void)
{
    g_wind_sensor.base.online = 0U;
    g_wind_sensor.base.last_update_tick = 0U;
    g_wind_sensor.speed_raw_adc = 0U;
    g_wind_sensor.direction_raw_adc = 0U;
    g_wind_sensor.wind_speed_mps = 0.0f;
    g_wind_sensor.wind_dir_deg = 0.0f;
}

void WindSensor_Poll(void)
{
    const uint16_t speed_raw = WindSensor_ReadAdcChannel(WIND_SPEED_ADC_CHANNEL);
    const uint16_t direction_raw = WindSensor_ReadAdcChannel(WIND_DIRECTION_ADC_CHANNEL);
    const float speed_voltage = WindSensor_AdcToVoltage(speed_raw);
    const float direction_voltage = WindSensor_AdcToVoltage(direction_raw);

    g_wind_sensor.speed_raw_adc = speed_raw;
    g_wind_sensor.direction_raw_adc = direction_raw;
    g_wind_sensor.wind_speed_mps = (speed_voltage / WIND_SENSOR_OUTPUT_MAX_VOLTAGE) * WIND_SPEED_FULL_SCALE_MPS;
    if (g_wind_sensor.wind_speed_mps < 0.0f)
    {
        g_wind_sensor.wind_speed_mps = 0.0f;
    }
    if (g_wind_sensor.wind_speed_mps > WIND_SPEED_FULL_SCALE_MPS)
    {
        g_wind_sensor.wind_speed_mps = WIND_SPEED_FULL_SCALE_MPS;
    }

    g_wind_sensor.wind_dir_deg = WindSensor_DecodeDirection(direction_voltage);

    g_weather_data.wind_speed_mps = g_wind_sensor.wind_speed_mps;
    g_weather_data.wind_dir_deg = g_wind_sensor.wind_dir_deg;
    g_weather_data.wind_online = 1U;
    g_wind_sensor.base.online = 1U;
    g_wind_sensor.base.last_update_tick = HAL_GetTick();
}

void WindSensor_Parse(void)
{
    /* Analog wind sensors do not need UART frame parsing. */
}
