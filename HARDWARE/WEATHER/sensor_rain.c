#include "sensor_rain.h"
#include "bsp_uart.h"
#include "weather_data.h"
#include "stm32h7xx_hal.h"

RainSensor_t g_rain_sensor = {0};

static uint8_t rain_buf[32];

void RainSensor_Init(void)
{
    g_rain_sensor.base.online = 0;
}

void RainSensor_Poll(void)
{
    // 如果是查询式串口雨滴模块，在这里发命令
    // uint8_t cmd[] = {0xA5, 0x01, 0x00, 0xA6};
    // BSP_Uart_Send(&g_uart_rain, cmd, sizeof(cmd));
}

void RainSensor_Parse(void)
{
    uint16_t len = UartRingBuf_Available(&g_uart_rain.rx_rb);
    if(len == 0) return;
    if(len > sizeof(rain_buf)) len = sizeof(rain_buf);

    UartRingBuf_Read(&g_uart_rain.rx_rb, rain_buf, len);

    // 这里按你的雨滴模块协议改
    // 示例：假设收到 ASCII: "RAIN:123\r\n"
    if(len >= 8)
    {
        // 占位示例
        g_rain_sensor.rain_value = 123.0f;
        g_rain_sensor.rain_detected = (g_rain_sensor.rain_value > 50.0f) ? 1 : 0;

        g_weather_data.rain_value = g_rain_sensor.rain_value;
        g_weather_data.rain_detected = g_rain_sensor.rain_detected;
        g_weather_data.rain_online = 1;

        g_rain_sensor.base.last_update_tick = HAL_GetTick();
        g_rain_sensor.base.online = 1;
    }
}
