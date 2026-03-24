#ifndef __WEATHER_DATA_H
#define __WEATHER_DATA_H

#include <stdint.h>

#define WEATHER_TEXT_LEN 16U

typedef struct
{
    float wind_speed_mps;
    float wind_dir_deg;
    uint16_t wind_speed_raw;
    uint16_t wind_direction_raw;
    char adc_mode_text[WEATHER_TEXT_LEN];
    float wind_speed_voltage;
    float wind_direction_voltage;
    float temperature_c;
    float humidity_rh;
    float co2_ppm;
    float pm25_ugm3;
    float pm10_ugm3;
    float tvoc_mg_m3;
    float ch2o_mg_m3;
    float rain_value;
    uint16_t rain_adc_raw;
    float rain_voltage;
    uint8_t rain_detected;
    char wind_direction_text[WEATHER_TEXT_LEN];
    char rain_level_text[WEATHER_TEXT_LEN];

    uint8_t wind_online;
    uint8_t air_online;
    uint8_t rain_online;
} WeatherData_t;

extern WeatherData_t g_weather_data;

#endif
