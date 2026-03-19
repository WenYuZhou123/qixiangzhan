#include "sensor_air_cj702.h"
#include "bsp_uart.h"
#include "weather_data.h"
#include "stm32h7xx_hal.h"
#include <stdio.h>
#include <string.h>

#define CJ702_FRAME_LEN 17U
#define CJ702_HEADER1   0x3CU
#define CJ702_HEADER2   0x02U

AirSensor_t g_air_sensor = {0};

static uint8_t air_rx_cache[64];
static uint16_t air_rx_cache_len = 0U;
static char air_last_frame_hex[128];

static void AirSensor_SaveFrameHex(const uint8_t *buf, uint16_t len)
{
    uint16_t i = 0U;
    uint16_t pos = 0U;

    if (buf == 0)
    {
        air_last_frame_hex[0] = '\0';
        return;
    }

    for (i = 0U; (i < len) && (pos + 4U < sizeof(air_last_frame_hex)); ++i)
    {
        pos += (uint16_t)snprintf(&air_last_frame_hex[pos], sizeof(air_last_frame_hex) - pos, "%02X ", buf[i]);
    }

    if ((pos > 0U) && (pos < sizeof(air_last_frame_hex)))
    {
        air_last_frame_hex[pos - 1U] = '\0';
    }
    else
    {
        air_last_frame_hex[sizeof(air_last_frame_hex) - 1U] = '\0';
    }
}

static uint8_t AirSensor_CheckFrame(const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint8_t checksum = 0U;

    if (len != CJ702_FRAME_LEN) return 0U;
    if (buf[0] != CJ702_HEADER1 || buf[1] != CJ702_HEADER2) return 0U;

    for (i = 0; i < (CJ702_FRAME_LEN - 1U); ++i)
    {
        checksum = (uint8_t)(checksum + buf[i]);
    }

    return (uint8_t)((checksum == buf[CJ702_FRAME_LEN - 1U]) ? 1U : 0U);
}

static void AirSensor_DecodeFrame(const uint8_t *buf)
{
    const uint16_t eco2_raw = (uint16_t)((buf[2] << 8) | buf[3]);
    const uint16_t ech2o_raw = (uint16_t)((buf[4] << 8) | buf[5]);
    const uint16_t tvoc_raw = (uint16_t)((buf[6] << 8) | buf[7]);
    const uint16_t pm25_raw = (uint16_t)((buf[8] << 8) | buf[9]);
    const uint16_t pm10_raw = (uint16_t)((buf[10] << 8) | buf[11]);
    const uint8_t temp_sign = (uint8_t)((buf[12] & 0x80U) != 0U);
    const float temp_abs = (float)(buf[12] & 0x7FU) + ((float)buf[13] / 10.0f);
    const float hum_value = (float)buf[14] + ((float)buf[15] / 10.0f);

    g_air_sensor.temperature_c = (temp_sign != 0U) ? (-temp_abs) : temp_abs;
    g_air_sensor.humidity_rh   = hum_value;
    g_air_sensor.co2_ppm       = (float)eco2_raw;
    g_air_sensor.pm25_ugm3     = (float)pm25_raw;
    g_air_sensor.pm10_ugm3     = (float)pm10_raw;
    g_air_sensor.tvoc_mg_m3    = (float)tvoc_raw / 1000.0f;
    g_air_sensor.ch2o_mg_m3    = (float)ech2o_raw / 1000.0f;

    g_weather_data.temperature_c = g_air_sensor.temperature_c;
    g_weather_data.humidity_rh   = g_air_sensor.humidity_rh;
    g_weather_data.co2_ppm       = g_air_sensor.co2_ppm;
    g_weather_data.pm25_ugm3     = g_air_sensor.pm25_ugm3;
    g_weather_data.pm10_ugm3     = g_air_sensor.pm10_ugm3;
    g_weather_data.tvoc_mg_m3    = g_air_sensor.tvoc_mg_m3;
    g_weather_data.ch2o_mg_m3    = g_air_sensor.ch2o_mg_m3;
    g_weather_data.air_online    = 1U;
}

void AirSensor_Init(void)
{
    g_air_sensor.base.online = 0U;
    g_air_sensor.base.last_update_tick = 0U;
    air_rx_cache_len = 0U;
    air_last_frame_hex[0] = '\0';
}

void AirSensor_Poll(void)
{
    /* CJ702-U reports automatically every 2 seconds over UART/485. */
}

void AirSensor_Parse(void)
{
    uint8_t tmp[32];
    uint16_t available = UartRingBuf_Available(&g_uart_air.rx_rb);
    uint16_t to_read;
    uint16_t i;

    if (available == 0U) return;

    to_read = (available > sizeof(tmp)) ? (uint16_t)sizeof(tmp) : available;
    to_read = UartRingBuf_Read(&g_uart_air.rx_rb, tmp, to_read);

    for (i = 0; i < to_read; ++i)
    {
        if (air_rx_cache_len < sizeof(air_rx_cache))
        {
            air_rx_cache[air_rx_cache_len++] = tmp[i];
        }
        else
        {
            memmove(air_rx_cache, air_rx_cache + 1, sizeof(air_rx_cache) - 1U);
            air_rx_cache[sizeof(air_rx_cache) - 1U] = tmp[i];
        }
    }

    while (air_rx_cache_len >= CJ702_FRAME_LEN)
    {
        uint16_t start = 0U;

        while ((start + 1U) < air_rx_cache_len)
        {
            if (air_rx_cache[start] == CJ702_HEADER1 && air_rx_cache[start + 1U] == CJ702_HEADER2)
            {
                break;
            }
            start++;
        }

        if (start > 0U)
        {
            memmove(air_rx_cache, air_rx_cache + start, air_rx_cache_len - start);
            air_rx_cache_len -= start;
        }

        if (air_rx_cache_len < CJ702_FRAME_LEN)
        {
            break;
        }

        if (AirSensor_CheckFrame(air_rx_cache, CJ702_FRAME_LEN) != 0U)
        {
            AirSensor_SaveFrameHex(air_rx_cache, CJ702_FRAME_LEN);
            AirSensor_DecodeFrame(air_rx_cache);
            g_air_sensor.base.last_update_tick = HAL_GetTick();
            g_air_sensor.base.online = 1U;
            memmove(air_rx_cache, air_rx_cache + CJ702_FRAME_LEN, air_rx_cache_len - CJ702_FRAME_LEN);
            air_rx_cache_len -= CJ702_FRAME_LEN;
        }
        else
        {
            memmove(air_rx_cache, air_rx_cache + 1, air_rx_cache_len - 1U);
            air_rx_cache_len -= 1U;
        }
    }
}

const char *AirSensor_LastFrameHex(void)
{
    return air_last_frame_hex;
}
