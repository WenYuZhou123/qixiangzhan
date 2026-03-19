#include "app_main.h"

#include "usart.h"
#include "relay.h"
#include "l610.h"
#include "l610_mqtt.h"
#include "app_weather.h"
#include "weather_data.h"
#include "sensor_air_cj702.h"
#include "sensor_wind_zts.h"
#include "sensor_rain.h"
#include "lcd_debug_ui.h"

#include <stdio.h>
#include <string.h>

#define APP_MQTT_RECONNECT_INTERVAL_MS 5000U

static uint32_t app_mqtt_last_retry_tick = 0U;

static void App_PrintText(const char *text)
{
    (void)text;
}

static const char *App_GetL610StatusText(L610_Status_t status)
{
    switch (status)
    {
        case L610_OK:
            return "OK";
        case L610_TIMEOUT:
            return "TIMEOUT";
        default:
            return "ERROR";
    }
}

static void App_PrintDiagBanner(L610_Status_t sync_status)
{
    L610_MQTT_Config_t mqtt_config;
    char msg[160];

    App_PrintText("\r\nL610 diagnostic mode ready\r\n");
    snprintf(msg, sizeof(msg), "Modem sync: %s\r\n", App_GetL610StatusText(sync_status));
    App_PrintText(msg);
    App_PrintText("MQTT auto reconnect is enabled\r\n");
    L610_MQTT_GetConfig(&mqtt_config);
    snprintf(msg, sizeof(msg), "CMD: %s\r\n", mqtt_config.topic_cmd);
    App_PrintText(msg);
}

static void App_PrintMQTTStage(const char *stage, L610_MQTT_Status_t status)
{
    char msg[80];

    snprintf(msg, sizeof(msg), "[APP] %s: %s\r\n", stage, L610_MQTT_GetStatusString(status));
    App_PrintText(msg);
}

void App_FillDeviceStatus(App_DeviceStatus_t *status)
{
    L610_Info_t info;

    if (status == NULL)
    {
        return;
    }

    memset(status, 0, sizeof(*status));
    strncpy(status->device_id, MQTT_CLIENT_ID, sizeof(status->device_id) - 1U);
    status->online = (uint8_t)((L610_MQTT_IsConnected() != 0) ? 1U : 0U);
    status->tick = HAL_GetTick();
    status->relay1 = (uint8_t)((Relay_GetState(RELAY1) == RELAY_ON) ? 1U : 0U);
    status->relay2 = (uint8_t)((Relay_GetState(RELAY2) == RELAY_ON) ? 1U : 0U);

    strncpy(status->pad.left_state, status->relay1 ? "open" : "closed", sizeof(status->pad.left_state) - 1U);
    strncpy(status->pad.right_state, status->relay2 ? "open" : "closed", sizeof(status->pad.right_state) - 1U);
    status->pad.ready = 0U;
    status->pad.occupied = 0U;
    strncpy(status->pad.mode, "auto", sizeof(status->pad.mode) - 1U);

    status->weather.wind_speed = g_weather_data.wind_speed_mps;
    status->weather.wind_direction = g_weather_data.wind_dir_deg;
    status->weather.temperature = g_weather_data.temperature_c;
    status->weather.humidity = g_weather_data.humidity_rh;
    status->weather.pressure = 0.0f;
    status->weather.visibility = 0.0f;
    status->weather.rain_detected = g_weather_data.rain_detected;
    status->weather.rain_value = g_weather_data.rain_value;
    status->weather.pm25 = g_weather_data.pm25_ugm3;
    status->weather.pm10 = g_weather_data.pm10_ugm3;
    status->weather.co2 = g_weather_data.co2_ppm;
    status->weather.tvoc = g_weather_data.tvoc_mg_m3;
    status->weather.ch2o = g_weather_data.ch2o_mg_m3;
    status->weather.wind_adc_raw = g_wind_sensor.speed_raw_adc;
    status->weather.direction_adc_raw = g_wind_sensor.direction_raw_adc;
    status->weather.cj702_online = g_air_sensor.base.online;
    status->weather.wind_online = g_weather_data.wind_online;
    status->weather.rain_online = g_weather_data.rain_online;
    status->alarm_count = 0U;
    status->last_status_publish_ok = L610_MQTT_GetLastStatusPublishOk();
    status->last_status_publish_tick = L610_MQTT_GetLastStatusPublishTick();
    strncpy(status->last_error_text,
            (L610_MQTT_IsConnected() != 0) ? "OK" : L610_MQTT_GetStateString(),
            sizeof(status->last_error_text) - 1U);
    strncpy(status->last_cj702_frame_hex,
            (AirSensor_LastFrameHex()[0] != '\0') ? AirSensor_LastFrameHex() : "N/A",
            sizeof(status->last_cj702_frame_hex) - 1U);

    strncpy(status->state_text, "running", sizeof(status->state_text) - 1U);
    if (L610_GetCachedInfo(&info) == L610_OK)
    {
        status->net.rssi = info.rssi;
        strncpy(status->net.operator_name, info.operator_name, sizeof(status->net.operator_name) - 1U);
        strncpy(status->net.ip, info.ip_addr, sizeof(status->net.ip) - 1U);
    }
}

static void App_MQTTBringUp(void)
{
    L610_MQTT_Status_t status;
    L610_MQTT_State_t state;

    state = L610_MQTT_GetState();

    if (state < MQTT_STATE_NET_READY)
    {
        status = L610_MQTT_PrepareNet();
        if (status != L610_MQTT_OK)
        {
            App_PrintMQTTStage("PrepareNet", status);
            return;
        }
    }

    if (L610_MQTT_GetState() < MQTT_STATE_CONNECTED)
    {
        status = L610_MQTT_Connect();
        if (status != L610_MQTT_OK)
        {
            App_PrintMQTTStage("Connect", status);
            return;
        }
    }

    if (L610_MQTT_GetState() < MQTT_STATE_SUBSCRIBED)
    {
        status = L610_MQTT_SubscribeCmd();
        App_PrintMQTTStage("Subscribe", status);
    }
}

void App_MainInit(void)
{
    L610_Status_t l610_sync_status = L610_TIMEOUT;

    Relay_Init();
    L610_Init();
    L610_MQTT_Init();
    AppWeather_Init();
    LCD_DebugUI_Init();
    HAL_Delay(3000);
    l610_sync_status = L610_Sync(1);
    App_PrintDiagBanner(l610_sync_status);
    app_mqtt_last_retry_tick = 0U;
}

void App_MainTask(void)
{
    L610_MQTT_Task();
    AppWeather_Task();
    LCD_DebugUI_Task();

    if (L610_MQTT_GetState() >= MQTT_STATE_SUBSCRIBED)
    {
        app_mqtt_last_retry_tick = HAL_GetTick();
        return;
    }

    if (app_mqtt_last_retry_tick == 0U ||
        (HAL_GetTick() - app_mqtt_last_retry_tick) >= APP_MQTT_RECONNECT_INTERVAL_MS)
    {
        app_mqtt_last_retry_tick = HAL_GetTick();
        App_MQTTBringUp();
    }
}

uint8_t App_MQTTIsReady(void)
{
    return (uint8_t)((L610_MQTT_IsReady() != 0) ? 1U : 0U);
}

void App_GetRuntimeStatus(App_RuntimeStatus_t *status)
{
    if (status == NULL)
    {
        return;
    }

    status->mqtt_ready = App_MQTTIsReady();
    status->mqtt_state = L610_MQTT_GetState();
    status->relay1_state = Relay_GetState(RELAY1);
    status->relay2_state = Relay_GetState(RELAY2);
    status->tick = HAL_GetTick();
}

L610_MQTT_Status_t App_RequestStatusSync(void)
{
    return L610_MQTT_RequestStatusRefresh();
}
