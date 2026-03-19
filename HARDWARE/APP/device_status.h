#ifndef DEVICE_STATUS_H
#define DEVICE_STATUS_H

#include <stdint.h>

#define APP_STATUS_TEXT_LEN     128U
#define APP_STATUS_FIELD_LEN    32U

typedef struct
{
    char left_state[APP_STATUS_FIELD_LEN];
    char right_state[APP_STATUS_FIELD_LEN];
    uint8_t ready;
    uint8_t occupied;
    char mode[APP_STATUS_FIELD_LEN];
} App_PadStatus_t;

typedef struct
{
    float wind_speed;
    float wind_direction;
    float temperature;
    float humidity;
    float pressure;
    float visibility;
    uint8_t rain_detected;
    float rain_value;
    float pm25;
    float pm10;
    float co2;
    float tvoc;
    float ch2o;
} App_WeatherStatus_t;

typedef struct
{
    int rssi;
    char operator_name[APP_STATUS_FIELD_LEN];
    char ip[APP_STATUS_FIELD_LEN];
} App_NetStatus_t;

typedef struct
{
    char device_id[APP_STATUS_FIELD_LEN];
    uint8_t online;
    uint32_t tick;
    uint8_t relay1;
    uint8_t relay2;
    App_PadStatus_t pad;
    App_WeatherStatus_t weather;
    App_NetStatus_t net;
    char state_text[APP_STATUS_TEXT_LEN];
} App_DeviceStatus_t;

void App_FillDeviceStatus(App_DeviceStatus_t *status);

#endif
