#ifndef __WEATHER_DATA_H
#define __WEATHER_DATA_H

#include <stdint.h>

typedef struct
{
    float wind_speed_mps;      // m/s
    float wind_dir_deg;        // 0~360
    float temperature_c;       // ℃
    float humidity_rh;         // %RH
    float co2_ppm;             // ppm
    float pm25_ugm3;           // ug/m3
    float tvoc_mg_m3;          // mg/m3
    float ch2o_mg_m3;          // mg/m3
    float rain_value;          // 自定义，视传感器而定
    uint8_t rain_detected;     // 0/1

    uint8_t wind_online;
    uint8_t air_online;
    uint8_t rain_online;
} WeatherData_t;

extern WeatherData_t g_weather_data;

#endif
