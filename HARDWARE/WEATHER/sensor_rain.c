#include "sensor_rain.h"
#include "main.h"
#include "weather_data.h"
#include "stm32h7xx_hal.h"

#ifndef RAIN_DO_GPIO_Port
#define RAIN_DO_GPIO_Port GPIOB
#endif

#ifndef RAIN_DO_Pin
#define RAIN_DO_Pin GPIO_PIN_7
#endif

#define RAIN_SENSOR_ACTIVE_LEVEL      GPIO_PIN_RESET
#define RAIN_SENSOR_INACTIVE_LEVEL    GPIO_PIN_SET
#define RAIN_SENSOR_DEBOUNCE_COUNT    3U

RainSensor_t g_rain_sensor = {0};

static GPIO_PinState s_last_sample = RAIN_SENSOR_INACTIVE_LEVEL;
static uint8_t s_stable_count = 0U;

void RainSensor_Init(void)
{
    g_rain_sensor.base.online = 1U;
    g_rain_sensor.base.last_update_tick = HAL_GetTick();
    g_rain_sensor.rain_value = 0.0f;
    g_rain_sensor.rain_detected = 0U;
    g_weather_data.rain_online = 1U;
}

void RainSensor_Poll(void)
{
    GPIO_PinState sample = HAL_GPIO_ReadPin(RAIN_DO_GPIO_Port, RAIN_DO_Pin);

    if (sample == s_last_sample)
    {
        if (s_stable_count < RAIN_SENSOR_DEBOUNCE_COUNT)
        {
            s_stable_count++;
        }
    }
    else
    {
        s_last_sample = sample;
        s_stable_count = 1U;
    }

    if (s_stable_count >= RAIN_SENSOR_DEBOUNCE_COUNT)
    {
        const uint8_t detected = (uint8_t)((sample == RAIN_SENSOR_ACTIVE_LEVEL) ? 1U : 0U);
        g_rain_sensor.rain_detected = detected;
        g_rain_sensor.rain_value = (detected != 0U) ? 1.0f : 0.0f;
        g_rain_sensor.base.last_update_tick = HAL_GetTick();
        g_weather_data.rain_detected = detected;
        g_weather_data.rain_value = g_rain_sensor.rain_value;
        g_weather_data.rain_online = 1U;
    }
}

void RainSensor_Parse(void)
{
    /* Digital rain sensor uses GPIO polling only. */
}
