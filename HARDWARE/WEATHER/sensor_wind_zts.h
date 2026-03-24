#ifndef __SENSOR_WIND_ZTS_H
#define __SENSOR_WIND_ZTS_H

#include "sensor_common.h"
#include "weather_data.h"

#define WIND_SENSOR_FRAME_TEXT_LEN  64U

typedef struct
{
    SensorBase_t base;
    uint16_t speed_raw_reg;
    uint16_t direction_raw_reg;
    uint16_t last_query_raw;
    float wind_speed_mps;
    float wind_dir_deg;
    char direction_text[WEATHER_TEXT_LEN];
    uint8_t speed_failures;
    uint8_t direction_failures;
    uint8_t last_query_failures;
    uint8_t current_query_is_direction;
    uint32_t last_poll_tick;
    uint32_t speed_last_ok_tick;
    uint32_t direction_last_ok_tick;
    char last_tx_hex[WIND_SENSOR_FRAME_TEXT_LEN];
    char last_rx_hex[WIND_SENSOR_FRAME_TEXT_LEN];
} WindSensor_t;

extern WindSensor_t g_wind_sensor;

void WindSensor_Init(void);
void WindSensor_Poll(void);
void WindSensor_Parse(void);

#endif
