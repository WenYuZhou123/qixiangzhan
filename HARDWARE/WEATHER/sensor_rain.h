#ifndef __SENSOR_RAIN_H
#define __SENSOR_RAIN_H

#include "sensor_common.h"
#include "weather_data.h"

typedef struct
{
    SensorBase_t base;
    uint16_t rain_adc_raw;
    float rain_value;
    uint8_t rain_detected;
    char rain_level_text[WEATHER_TEXT_LEN];
} RainSensor_t;

extern RainSensor_t g_rain_sensor;

void RainSensor_Init(void);
void RainSensor_MarkOffline(void);
void RainSensor_Poll(void);
void RainSensor_Parse(void);

#endif
