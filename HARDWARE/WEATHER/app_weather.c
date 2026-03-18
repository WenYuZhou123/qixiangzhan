#include "app_weather.h"
#include "bsp_uart.h"
#include "sensor_wind_zts.h"
#include "sensor_air_cj702.h"
#include "sensor_rain.h"
#include "stm32h7xx_hal.h"

static uint32_t tick_wind = 0;
static uint32_t tick_air  = 0;
static uint32_t tick_rain = 0;

void AppWeather_Init(void)
{
    BSP_Uart_Init();
    WindSensor_Init();
    AirSensor_Init();
    RainSensor_Init();
}

void AppWeather_Task(void)
{
    uint32_t now = HAL_GetTick();

    if(now - tick_wind >= 1000)
    {
        tick_wind = now;
        WindSensor_Poll();
    }

    if(now - tick_air >= 1000)
    {
        tick_air = now;
        AirSensor_Poll();
    }

    if(now - tick_rain >= 1000)
    {
        tick_rain = now;
        RainSensor_Poll();
    }

    AirSensor_Parse();
    RainSensor_Parse();
}
