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
#define APP_L610_PROBE_INTERVAL_MS      300U
#define APP_L610_RETRY_INTERVAL_MS      1500U

typedef enum
{
    APP_L610_STAGE_AT = 0,
    APP_L610_STAGE_ATE0,
    APP_L610_STAGE_ATI,
    APP_L610_STAGE_CPIN,
    APP_L610_STAGE_CSQ,
    APP_L610_STAGE_CREG,
    APP_L610_STAGE_CGREG,
    APP_L610_STAGE_CGATT,
    APP_L610_STAGE_COPS,
    APP_L610_STAGE_CGPADDR,
    APP_L610_STAGE_DONE
} App_L610DiagStage_t;

typedef struct
{
    App_L610DiagStage_t stage;
    uint8_t complete;
    uint8_t at_ready;
    uint8_t sim_ready;
    uint8_t creg;
    uint8_t cgreg;
    uint8_t cgatt;
    int rssi;
    int ber;
    uint32_t timeout_count;
    uint32_t next_probe_tick;
    char last_cmd[APP_STATUS_FIELD_LEN];
    char last_resp[APP_STATUS_TEXT_LEN];
    char operator_name[APP_STATUS_FIELD_LEN];
    char ip_addr[APP_STATUS_FIELD_LEN];
} App_L610Diag_t;

static uint32_t app_mqtt_last_retry_tick = 0U;
static App_L610Diag_t app_l610_diag;

static void App_PrintText(const char *text)
{
    if (text != NULL)
    {
        HAL_UART_Transmit(&huart2, (uint8_t *)text, strlen(text), 1000);
    }
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

static void App_L610Diag_Reset(void)
{
    memset(&app_l610_diag, 0, sizeof(app_l610_diag));
    app_l610_diag.stage = APP_L610_STAGE_AT;
}

static void App_L610Diag_SetLast(const char *cmd, L610_Status_t status)
{
    char *buf = L610_GetBuffer();

    if (cmd != NULL)
    {
        strncpy(app_l610_diag.last_cmd, cmd, sizeof(app_l610_diag.last_cmd) - 1U);
        app_l610_diag.last_cmd[sizeof(app_l610_diag.last_cmd) - 1U] = '\0';
    }

    if (buf != NULL && buf[0] != '\0')
    {
        strncpy(app_l610_diag.last_resp, buf, sizeof(app_l610_diag.last_resp) - 1U);
        app_l610_diag.last_resp[sizeof(app_l610_diag.last_resp) - 1U] = '\0';
    }

    if (status == L610_TIMEOUT)
    {
        app_l610_diag.timeout_count++;
    }
}

static void App_L610Diag_Advance(uint8_t success)
{
    if (success != 0U)
    {
        app_l610_diag.stage = (App_L610DiagStage_t)((uint8_t)app_l610_diag.stage + 1U);
        app_l610_diag.next_probe_tick = HAL_GetTick() + APP_L610_PROBE_INTERVAL_MS;
    }
    else if (app_l610_diag.stage == APP_L610_STAGE_AT)
    {
        app_l610_diag.next_probe_tick = HAL_GetTick() + APP_L610_RETRY_INTERVAL_MS;
    }
    else
    {
        app_l610_diag.stage = (App_L610DiagStage_t)((uint8_t)app_l610_diag.stage + 1U);
        app_l610_diag.next_probe_tick = HAL_GetTick() + APP_L610_PROBE_INTERVAL_MS;
    }

    if (app_l610_diag.stage >= APP_L610_STAGE_DONE)
    {
        app_l610_diag.complete = 1U;
    }
}

static void App_L610DiagTask(void)
{
    L610_Status_t l610_status = L610_ERROR;
    uint8_t tmp_u8 = 0U;
    int rssi = 0;
    int ber = 0;

    if (app_l610_diag.complete != 0U)
    {
        return;
    }

    if (HAL_GetTick() < app_l610_diag.next_probe_tick)
    {
        return;
    }

    switch (app_l610_diag.stage)
    {
    case APP_L610_STAGE_AT:
        l610_status = L610_SendRawCommand("AT", 1500U, 100U);
        App_L610Diag_SetLast("AT", l610_status);
        app_l610_diag.at_ready = (uint8_t)((l610_status == L610_OK) ? 1U : 0U);
        App_L610Diag_Advance((uint8_t)((l610_status == L610_OK) ? 1U : 0U));
        break;

    case APP_L610_STAGE_ATE0:
        l610_status = L610_SendRawCommand("ATE0", 1500U, 100U);
        App_L610Diag_SetLast("ATE0", l610_status);
        App_L610Diag_Advance(1U);
        break;

    case APP_L610_STAGE_ATI:
        l610_status = L610_SendRawCommand("ATI", 2000U, 150U);
        App_L610Diag_SetLast("ATI", l610_status);
        App_L610Diag_Advance(1U);
        break;

    case APP_L610_STAGE_CPIN:
        l610_status = L610_SendRawCommand("AT+CPIN?", 1500U, 100U);
        App_L610Diag_SetLast("AT+CPIN?", l610_status);
        if ((l610_status == L610_OK) && (strstr(L610_GetBuffer(), "READY") != NULL))
        {
            app_l610_diag.sim_ready = 1U;
        }
        App_L610Diag_Advance(1U);
        break;

    case APP_L610_STAGE_CSQ:
        l610_status = L610_GetCSQ(&rssi, &ber);
        App_L610Diag_SetLast("AT+CSQ", l610_status);
        if (l610_status == L610_OK)
        {
            app_l610_diag.rssi = rssi;
            app_l610_diag.ber = ber;
        }
        App_L610Diag_Advance(1U);
        break;

    case APP_L610_STAGE_CREG:
        l610_status = L610_CheckCREG(&tmp_u8);
        App_L610Diag_SetLast("AT+CREG?", l610_status);
        if (l610_status == L610_OK)
        {
            app_l610_diag.creg = tmp_u8;
        }
        App_L610Diag_Advance(1U);
        break;

    case APP_L610_STAGE_CGREG:
        l610_status = L610_CheckCGREG(&tmp_u8);
        App_L610Diag_SetLast("AT+CGREG?", l610_status);
        if (l610_status == L610_OK)
        {
            app_l610_diag.cgreg = tmp_u8;
        }
        App_L610Diag_Advance(1U);
        break;

    case APP_L610_STAGE_CGATT:
        l610_status = L610_CheckCGATT(&tmp_u8);
        App_L610Diag_SetLast("AT+CGATT?", l610_status);
        if (l610_status == L610_OK)
        {
            app_l610_diag.cgatt = tmp_u8;
        }
        App_L610Diag_Advance(1U);
        break;

    case APP_L610_STAGE_COPS:
        l610_status = L610_GetOperator(app_l610_diag.operator_name, sizeof(app_l610_diag.operator_name));
        App_L610Diag_SetLast("AT+COPS?", l610_status);
        App_L610Diag_Advance(1U);
        break;

    case APP_L610_STAGE_CGPADDR:
        l610_status = L610_GetIPAddress(app_l610_diag.ip_addr, sizeof(app_l610_diag.ip_addr));
        App_L610Diag_SetLast("AT+CGPADDR", l610_status);
        App_L610Diag_Advance(1U);
        break;

    case APP_L610_STAGE_DONE:
    default:
        app_l610_diag.complete = 1U;
        break;
    }
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
    status->weather.wind_valid = g_wind_sensor.valid;
    strncpy(status->weather.wind_invalid_reason, g_wind_sensor.invalid_reason, sizeof(status->weather.wind_invalid_reason) - 1U);
    status->alarm_count = 0U;
    status->last_status_publish_ok = L610_MQTT_GetLastStatusPublishOk();
    status->last_status_publish_tick = L610_MQTT_GetLastStatusPublishTick();
    status->lcd_refresh_tick = LCD_DebugUI_GetRefreshTick();
    status->lcd_refresh_count = LCD_DebugUI_GetRefreshCount();
    status->l610_at_ready = app_l610_diag.at_ready;
    status->l610_sim_ready = app_l610_diag.sim_ready;
    status->l610_creg = app_l610_diag.creg;
    status->l610_cgreg = app_l610_diag.cgreg;
    status->l610_cgatt = app_l610_diag.cgatt;
    status->l610_ber = app_l610_diag.ber;
    status->l610_timeout_count = app_l610_diag.timeout_count;
    strncpy(status->l610_last_cmd, app_l610_diag.last_cmd, sizeof(status->l610_last_cmd) - 1U);
    strncpy(status->l610_last_resp, app_l610_diag.last_resp, sizeof(status->l610_last_resp) - 1U);
    strncpy(status->l610_mqtt_stage, L610_MQTT_GetStateString(), sizeof(status->l610_mqtt_stage) - 1U);
    if (app_l610_diag.rssi != 0)
    {
        status->net.rssi = app_l610_diag.rssi;
    }
    if (app_l610_diag.operator_name[0] != '\0')
    {
        strncpy(status->net.operator_name, app_l610_diag.operator_name, sizeof(status->net.operator_name) - 1U);
    }
    if (app_l610_diag.ip_addr[0] != '\0')
    {
        strncpy(status->net.ip, app_l610_diag.ip_addr, sizeof(status->net.ip) - 1U);
    }
    if (app_l610_diag.at_ready == 0U)
    {
        strncpy(status->last_error_text, "L610 AT not ready", sizeof(status->last_error_text) - 1U);
    }
    else if (g_wind_sensor.valid == 0U)
    {
        strncpy(status->last_error_text, g_wind_sensor.invalid_reason, sizeof(status->last_error_text) - 1U);
    }
    else
    {
        strncpy(status->last_error_text,
                (L610_MQTT_IsConnected() != 0) ? "OK" : L610_MQTT_GetStateString(),
                sizeof(status->last_error_text) - 1U);
    }
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
    Relay_Init();
    L610_Init();
    L610_MQTT_Init();
    AppWeather_Init();
    LCD_DebugUI_Init();
    HAL_Delay(3000);
    App_L610Diag_Reset();
    App_PrintDiagBanner(L610_TIMEOUT);
    app_mqtt_last_retry_tick = 0U;
}

void App_MainTask(void)
{
    AppWeather_Task();
    App_L610DiagTask();
    LCD_DebugUI_Task();

    if (app_l610_diag.at_ready == 0U || app_l610_diag.complete == 0U)
    {
        return;
    }

    L610_MQTT_Task();

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
