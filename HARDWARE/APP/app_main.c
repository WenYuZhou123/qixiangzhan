#include "app_main.h"

#include "usart.h"
#include "relay.h"
#include "l610.h"
#include "l610_mqtt.h"
#include "app_weather.h"

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
    HAL_Delay(3000);
    l610_sync_status = L610_Sync(1);
    App_PrintDiagBanner(l610_sync_status);
    app_mqtt_last_retry_tick = 0U;
}

void App_MainTask(void)
{
    L610_MQTT_Task();
    AppWeather_Task();

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
