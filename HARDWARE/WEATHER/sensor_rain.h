#ifndef __SENSOR_RAIN_H
#define __SENSOR_RAIN_H

#include "sensor_common.h"

typedef struct
{
    SensorBase_t base;
    float rain_value;
    uint8_t rain_detected;
} RainSensor_t;

extern RainSensor_t g_rain_sensor;

void RainSensor_Init(void);
void RainSensor_Poll(void);
void RainSensor_Parse(void);

#endif
