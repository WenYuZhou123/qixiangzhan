#ifndef __SENSOR_WIND_ZTS_H
#define __SENSOR_WIND_ZTS_H

#include "sensor_common.h"

typedef struct
{
    SensorBase_t base;
    uint16_t speed_raw_adc;
    uint16_t direction_raw_adc;
    float wind_speed_mps;
    float wind_dir_deg;
} WindSensor_t;

extern WindSensor_t g_wind_sensor;

void WindSensor_Init(void);
void WindSensor_Poll(void);
void WindSensor_Parse(void);

#endif
