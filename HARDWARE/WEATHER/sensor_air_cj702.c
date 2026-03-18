#include "sensor_air_cj702.h"
#include "bsp_uart.h"
#include "weather_data.h"
#include "stm32h7xx_hal.h"

AirSensor_t g_air_sensor = {0};

static uint8_t air_frame[64];

static uint8_t AirSensor_CheckFrame(const uint8_t *buf, uint16_t len)
{
    // 这里按你的协议改
    // 示例：AA 55 ... checksum
    if(len < 10) return 0;
    if(buf[0] != 0xAA || buf[1] != 0x55) return 0;

    // TODO: checksum 校验
    return 1;
}

static void AirSensor_DecodeFrame(const uint8_t *buf)
{
    // 以下全是占位示例，必须按你的CJ702-U手册改
    // 假设:
    // buf[2..3] 温度*10
    // buf[4..5] 湿度*10
    // buf[6..7] CO2
    // buf[8..9] PM2.5
    // buf[10..11] TVOC*1000
    // buf[12..13] CH2O*1000

    uint16_t temp_raw = (buf[2] << 8) | buf[3];
    uint16_t hum_raw  = (buf[4] << 8) | buf[5];
    uint16_t co2_raw  = (buf[6] << 8) | buf[7];
    uint16_t pm25_raw = (buf[8] << 8) | buf[9];
    uint16_t tvoc_raw = (buf[10] << 8) | buf[11];
    uint16_t ch2o_raw = (buf[12] << 8) | buf[13];

    g_air_sensor.temperature_c = temp_raw / 10.0f;
    g_air_sensor.humidity_rh   = hum_raw / 10.0f;
    g_air_sensor.co2_ppm       = co2_raw;
    g_air_sensor.pm25_ugm3     = pm25_raw;
    g_air_sensor.tvoc_mg_m3    = tvoc_raw / 1000.0f;
    g_air_sensor.ch2o_mg_m3    = ch2o_raw / 1000.0f;

    g_weather_data.temperature_c = g_air_sensor.temperature_c;
    g_weather_data.humidity_rh   = g_air_sensor.humidity_rh;
    g_weather_data.co2_ppm       = g_air_sensor.co2_ppm;
    g_weather_data.pm25_ugm3     = g_air_sensor.pm25_ugm3;
    g_weather_data.tvoc_mg_m3    = g_air_sensor.tvoc_mg_m3;
    g_weather_data.ch2o_mg_m3    = g_air_sensor.ch2o_mg_m3;
    g_weather_data.air_online    = 1;
}

void AirSensor_Init(void)
{
    g_air_sensor.base.online = 0;
}

void AirSensor_Poll(void)
{
    // 如果CJ702-U是主动上报，这里可以不发命令
    // 如果是查询式，就在这里发查询命令
    // 示例：
    // uint8_t cmd[] = {0xAA, 0x55, 0x00, 0x00};
    // BSP_Uart_Send(&g_uart_air, cmd, sizeof(cmd));
}

void AirSensor_Parse(void)
{
    uint16_t len = UartRingBuf_Available(&g_uart_air.rx_rb);
    if(len == 0) return;
    if(len > sizeof(air_frame)) len = sizeof(air_frame);

    UartRingBuf_Read(&g_uart_air.rx_rb, air_frame, len);

    if(AirSensor_CheckFrame(air_frame, len))
    {
        AirSensor_DecodeFrame(air_frame);
        g_air_sensor.base.last_update_tick = HAL_GetTick();
        g_air_sensor.base.online = 1;
    }
}
