#ifndef __SENSOR_AIR_CJ702_H
#define __SENSOR_AIR_CJ702_H

#include "sensor_common.h"

typedef struct
{
    SensorBase_t base;
    float temperature_c;
    float humidity_rh;
    float co2_ppm;
    float pm25_ugm3;
    float pm10_ugm3;
    float tvoc_mg_m3;
    float ch2o_mg_m3;
} AirSensor_t;

extern AirSensor_t g_air_sensor;

void AirSensor_Init(void);
void AirSensor_Poll(void);
void AirSensor_Parse(void);
const char *AirSensor_LastFrameHex(void);

#endif
