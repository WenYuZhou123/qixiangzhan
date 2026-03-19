#ifndef __WEATHER_DATA_H
#define __WEATHER_DATA_H

#include <stdint.h>

typedef struct
{
    float wind_speed_mps;
    float wind_dir_deg;
    float temperature_c;
    float humidity_rh;
    float co2_ppm;
    float pm25_ugm3;
    float pm10_ugm3;
    float tvoc_mg_m3;
    float ch2o_mg_m3;
    float rain_value;
    uint8_t rain_detected;

    uint8_t wind_online;
    uint8_t air_online;
    uint8_t rain_online;
} WeatherData_t;

extern WeatherData_t g_weather_data;

#endif
