#include "app_main.h"

#include "usart.h"
#include "relay.h"
#include "l610.h"
#include "l610_mqtt.h"
#include "app_weather.h"
#include "weather_data.h"
#include "bsp_uart.h"
#include "sensor_air_cj702.h"
#include "sensor_wind_zts.h"
#include "sensor_rain.h"
#include "console.h"
#include "lcd_debug_ui.h"
#include "lcd_port.h"

#include <stdio.h>
#include <string.h>

#define APP_DIAG_LOG_ENABLE          1U
#define APP_FW_TAG                   "mqtt-mipcall-fix-uart6-v1"
#define APP_HEARTBEAT_LOG_ENABLE     0U
#define APP_MQTT_AUTO_RECONNECT_ENABLE 1U
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

static App_L610Diag_t app_l610_diag;
#if APP_MQTT_AUTO_RECONNECT_ENABLE
static uint32_t app_mqtt_last_retry_tick = 0U;
#endif
#if APP_HEARTBEAT_LOG_ENABLE
static uint32_t app_last_heartbeat_tick = 0U;
#endif

static const char *App_GetDiagStageName(App_L610DiagStage_t stage)
{
    switch (stage)
    {
    case APP_L610_STAGE_AT:
        return "AT";
    case APP_L610_STAGE_ATE0:
        return "ATE0";
    case APP_L610_STAGE_ATI:
        return "ATI";
    case APP_L610_STAGE_CPIN:
        return "CPIN";
    case APP_L610_STAGE_CSQ:
        return "CSQ";
    case APP_L610_STAGE_CREG:
        return "CREG";
    case APP_L610_STAGE_CGREG:
        return "CGREG";
    case APP_L610_STAGE_CGATT:
        return "CGATT";
    case APP_L610_STAGE_COPS:
        return "COPS";
    case APP_L610_STAGE_CGPADDR:
        return "CGPADDR";
    case APP_L610_STAGE_DONE:
    default:
        return "DONE";
    }
}

static void App_PrintText(const char *text)
{
#if APP_DIAG_LOG_ENABLE
    if (text != NULL)
    {
        HAL_UART_Transmit(&huart6, (uint8_t *)text, strlen(text), 1000);
    }
#else
    (void)text;
#endif
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
#if APP_MQTT_AUTO_RECONNECT_ENABLE
    App_PrintText("MQTT auto reconnect is enabled (paused while console is active)\r\n");
#else
    App_PrintText("MQTT auto reconnect is disabled in manual console mode\r\n");
#endif
    L610_MQTT_GetConfig(&mqtt_config);
    snprintf(msg, sizeof(msg), "CMD: %s\r\n", mqtt_config.topic_cmd);
    App_PrintText(msg);
}

#if APP_MQTT_AUTO_RECONNECT_ENABLE
static void App_PrintMQTTStage(const char *stage, L610_MQTT_Status_t status)
{
    char msg[256];

    snprintf(msg, sizeof(msg), "[APP] %s: %s (%s) last=%s\r\n",
             stage,
             L610_MQTT_GetStatusString(status),
             L610_MQTT_GetLastStageDetail(),
             L610_MQTT_GetLastLine());
    App_PrintText(msg);
}
#endif

static void App_PrintBootStage(const char *stage, const char *detail)
{
    char msg[96];

    snprintf(msg, sizeof(msg), "BOOT: %s %s\r\n", stage != NULL ? stage : "stage", detail != NULL ? detail : "");
    App_PrintText(msg);
}

static void App_PrintDiagStep(const char *cmd, L610_Status_t status)
{
    char msg[96];

    snprintf(msg, sizeof(msg), "[DIAG] %s -> %s\r\n",
             (cmd != NULL) ? cmd : "CMD",
             App_GetL610StatusText(status));
    App_PrintText(msg);
}

#if APP_HEARTBEAT_LOG_ENABLE
static void App_PrintHeartbeat(void)
{
    char msg[128];

    snprintf(msg, sizeof(msg),
             "[LOOP] stage=%s at=%u sim=%u cgatt=%u mqtt=%s adc=%s\r\n",
             App_GetDiagStageName(app_l610_diag.stage),
             (unsigned)app_l610_diag.at_ready,
             (unsigned)app_l610_diag.sim_ready,
             (unsigned)app_l610_diag.cgatt,
             L610_MQTT_GetStateString(),
             g_weather_data.adc_mode_text);
    App_PrintText(msg);
}
#endif

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
    char msg[80];

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

    snprintf(msg, sizeof(msg), "[DIAG] next=%s complete=%u\r\n",
             App_GetDiagStageName(app_l610_diag.stage),
             (unsigned)app_l610_diag.complete);
    App_PrintText(msg);
}

static void App_L610DiagTask(void)
{
    L610_Status_t l610_status = L610_ERROR;
    L610_SessionStatus_t session_status;
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

    session_status = L610_BeginSession(L610_OWNER_DIAG);
    if (session_status != L610_SESSION_OK)
    {
        return;
    }

    switch (app_l610_diag.stage)
    {
    case APP_L610_STAGE_AT:
        l610_status = L610_SendRawCommand("AT", 1500U, 100U);
        App_PrintDiagStep("AT", l610_status);
        App_L610Diag_SetLast("AT", l610_status);
        app_l610_diag.at_ready = (uint8_t)((l610_status == L610_OK) ? 1U : 0U);
        App_L610Diag_Advance((uint8_t)((l610_status == L610_OK) ? 1U : 0U));
        break;

    case APP_L610_STAGE_ATE0:
        l610_status = L610_SendRawCommand("ATE0", 1500U, 100U);
        App_PrintDiagStep("ATE0", l610_status);
        App_L610Diag_SetLast("ATE0", l610_status);
        App_L610Diag_Advance(1U);
        break;

    case APP_L610_STAGE_ATI:
        l610_status = L610_SendRawCommand("ATI", 2000U, 150U);
        App_PrintDiagStep("ATI", l610_status);
        App_L610Diag_SetLast("ATI", l610_status);
        App_L610Diag_Advance(1U);
        break;

    case APP_L610_STAGE_CPIN:
        l610_status = L610_SendRawCommand("AT+CPIN?", 1500U, 100U);
        App_PrintDiagStep("AT+CPIN?", l610_status);
        App_L610Diag_SetLast("AT+CPIN?", l610_status);
        if ((l610_status == L610_OK) && (strstr(L610_GetBuffer(), "READY") != NULL))
        {
            app_l610_diag.sim_ready = 1U;
        }
        App_L610Diag_Advance(1U);
        break;

    case APP_L610_STAGE_CSQ:
        l610_status = L610_GetCSQ(&rssi, &ber);
        App_PrintDiagStep("AT+CSQ", l610_status);
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
        App_PrintDiagStep("AT+CREG?", l610_status);
        App_L610Diag_SetLast("AT+CREG?", l610_status);
        if (l610_status == L610_OK)
        {
            app_l610_diag.creg = tmp_u8;
        }
        App_L610Diag_Advance(1U);
        break;

    case APP_L610_STAGE_CGREG:
        l610_status = L610_CheckCGREG(&tmp_u8);
        App_PrintDiagStep("AT+CGREG?", l610_status);
        App_L610Diag_SetLast("AT+CGREG?", l610_status);
        if (l610_status == L610_OK)
        {
            app_l610_diag.cgreg = tmp_u8;
        }
        App_L610Diag_Advance(1U);
        break;

    case APP_L610_STAGE_CGATT:
        l610_status = L610_CheckCGATT(&tmp_u8);
        App_PrintDiagStep("AT+CGATT?", l610_status);
        App_L610Diag_SetLast("AT+CGATT?", l610_status);
        if (l610_status == L610_OK)
        {
            app_l610_diag.cgatt = tmp_u8;
        }
        App_L610Diag_Advance(1U);
        break;

    case APP_L610_STAGE_COPS:
        l610_status = L610_GetOperator(app_l610_diag.operator_name, sizeof(app_l610_diag.operator_name));
        App_PrintDiagStep("AT+COPS?", l610_status);
        App_L610Diag_SetLast("AT+COPS?", l610_status);
        App_L610Diag_Advance(1U);
        break;

    case APP_L610_STAGE_CGPADDR:
        l610_status = L610_GetIPAddress(app_l610_diag.ip_addr, sizeof(app_l610_diag.ip_addr));
        App_PrintDiagStep("AT+CGPADDR", l610_status);
        App_L610Diag_SetLast("AT+CGPADDR", l610_status);
        App_L610Diag_Advance(1U);
        break;

    case APP_L610_STAGE_DONE:
    default:
        app_l610_diag.complete = 1U;
        break;
    }

    L610_EndSession(L610_OWNER_DIAG);
}

static uint32_t App_MaxU32(uint32_t a, uint32_t b)
{
    return (a > b) ? a : b;
}

static uint32_t App_GetWeatherLastOkTick(void)
{
    uint32_t tick = 0U;

    tick = App_MaxU32(tick, g_wind_sensor.speed_last_ok_tick);
    tick = App_MaxU32(tick, g_wind_sensor.direction_last_ok_tick);
    tick = App_MaxU32(tick, g_air_sensor.base.last_update_tick);
    tick = App_MaxU32(tick, g_rain_sensor.base.last_update_tick);
    return tick;
}

void App_FillDeviceStatus(App_DeviceStatus_t *status)
{
    L610_Info_t info;
    const char *mqtt_last_tx;
    const char *mqtt_last_rx;
    const char *mqtt_detail;
    int mqtt_result;
    char mqtt_detail_buf[APP_STATUS_FIELD_LEN];

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
    status->weather.wind_speed_raw = g_weather_data.wind_speed_raw;
    status->weather.wind_direction_raw = g_weather_data.wind_direction_raw;
    strncpy(status->weather.adc_mode_text, g_weather_data.adc_mode_text, sizeof(status->weather.adc_mode_text) - 1U);
    status->weather.wind_speed_voltage = g_weather_data.wind_speed_voltage;
    status->weather.wind_direction_voltage = g_weather_data.wind_direction_voltage;
    status->weather.temperature = g_weather_data.temperature_c;
    status->weather.humidity = g_weather_data.humidity_rh;
    status->weather.pressure = 0.0f;
    status->weather.visibility = 0.0f;
    status->weather.capability_wind = 1U;
    status->weather.capability_air = 1U;
    status->weather.capability_rain = 1U;
    status->weather.capability_pressure = 0U;
    status->weather.capability_visibility = 0U;
    status->weather.rain_detected = g_weather_data.rain_detected;
    status->weather.rain_value = g_weather_data.rain_value;
    status->weather.pm25 = g_weather_data.pm25_ugm3;
    status->weather.pm10 = g_weather_data.pm10_ugm3;
    status->weather.co2 = g_weather_data.co2_ppm;
    status->weather.tvoc = g_weather_data.tvoc_mg_m3;
    status->weather.ch2o = g_weather_data.ch2o_mg_m3;
    status->weather.rain_adc_raw = g_weather_data.rain_adc_raw;
    status->weather.rain_voltage = g_weather_data.rain_voltage;
    strncpy(status->weather.wind_direction_text,
            g_weather_data.wind_direction_text,
            sizeof(status->weather.wind_direction_text) - 1U);
    strncpy(status->weather.rain_level_text,
            g_weather_data.rain_level_text,
            sizeof(status->weather.rain_level_text) - 1U);
    strncpy(status->weather.wind_query_target,
            (g_wind_sensor.current_query_is_direction != 0U) ? "DIR" : "SPD",
            sizeof(status->weather.wind_query_target) - 1U);
    strncpy(status->weather.wind_last_tx_hex,
            g_wind_sensor.last_tx_hex,
            sizeof(status->weather.wind_last_tx_hex) - 1U);
    strncpy(status->weather.wind_last_rx_hex,
            g_wind_sensor.last_rx_hex,
            sizeof(status->weather.wind_last_rx_hex) - 1U);
    status->weather.wind_query_raw = g_wind_sensor.last_query_raw;
    status->weather.wind_query_failures = g_wind_sensor.last_query_failures;
    status->weather.wind_query_rx_count = BSP_Uart_GetRxCount(&g_uart_wind);
    status->weather.wind_query_error_count = BSP_Uart_GetRxErrorCount(&g_uart_wind);
    status->weather.cj702_online = g_air_sensor.base.online;
    status->weather.wind_online = g_weather_data.wind_online;
    status->weather.air_online = g_weather_data.air_online;
    status->weather.rain_online = g_weather_data.rain_online;
    status->weather.sensor_last_ok_tick = App_GetWeatherLastOkTick();
    status->weather.sensor_failure_count = (uint16_t)g_wind_sensor.speed_failures +
                                           (uint16_t)g_wind_sensor.direction_failures +
                                           (uint16_t)((g_weather_data.air_online == 0U) ? 1U : 0U) +
                                           (uint16_t)((g_weather_data.rain_online == 0U) ? 1U : 0U);
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
    mqtt_last_tx = L610_MQTT_GetLastTx();
    mqtt_last_rx = L610_MQTT_GetLastRx();
    mqtt_detail = L610_MQTT_GetLastStageDetail();
    mqtt_result = L610_MQTT_GetLastResultCode();
    if (mqtt_last_tx != NULL && mqtt_last_tx[0] != '\0')
    {
        strncpy(status->l610_last_cmd, mqtt_last_tx, sizeof(status->l610_last_cmd) - 1U);
    }
    else
    {
        strncpy(status->l610_last_cmd, app_l610_diag.last_cmd, sizeof(status->l610_last_cmd) - 1U);
    }
    if (mqtt_last_rx != NULL && mqtt_last_rx[0] != '\0')
    {
        strncpy(status->l610_last_resp, mqtt_last_rx, sizeof(status->l610_last_resp) - 1U);
    }
    else
    {
        strncpy(status->l610_last_resp, app_l610_diag.last_resp, sizeof(status->l610_last_resp) - 1U);
    }
    strncpy(status->l610_mqtt_stage, L610_MQTT_GetStateString(), sizeof(status->l610_mqtt_stage) - 1U);
    if (mqtt_detail != NULL && mqtt_detail[0] != '\0')
    {
        snprintf(mqtt_detail_buf, sizeof(mqtt_detail_buf), "%s:%d", mqtt_detail, mqtt_result);
        strncpy(status->l610_mqtt_detail, mqtt_detail_buf, sizeof(status->l610_mqtt_detail) - 1U);
    }
    else
    {
        snprintf(mqtt_detail_buf, sizeof(mqtt_detail_buf), "rc=%d", mqtt_result);
        strncpy(status->l610_mqtt_detail, mqtt_detail_buf, sizeof(status->l610_mqtt_detail) - 1U);
    }
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
    else if ((g_weather_data.wind_online == 0U) && (g_weather_data.rain_online == 0U))
    {
        strncpy(status->last_error_text, "WEATHER_OFFLINE", sizeof(status->last_error_text) - 1U);
    }
    else if (g_weather_data.wind_online == 0U)
    {
        strncpy(status->last_error_text, "WIND485_OFFLINE", sizeof(status->last_error_text) - 1U);
    }
    else if (g_weather_data.rain_online == 0U)
    {
        strncpy(status->last_error_text, "RAIN_ADC_OFFLINE", sizeof(status->last_error_text) - 1U);
    }
    else
    {
        strncpy(status->last_error_text,
                (L610_MQTT_IsConnected() != 0) ? "OK" : L610_MQTT_GetStateString(),
                sizeof(status->last_error_text) - 1U);
    }
    strncpy(status->weather.sensor_last_error,
            status->last_error_text,
            sizeof(status->weather.sensor_last_error) - 1U);
    strncpy(status->last_cj702_frame_hex,
            (AirSensor_LastFrameHex()[0] != '\0') ? AirSensor_LastFrameHex() : "N/A",
            sizeof(status->last_cj702_frame_hex) - 1U);

    snprintf(status->state_text, sizeof(status->state_text), "weather=%s mqtt=%s", g_weather_data.adc_mode_text, L610_MQTT_GetStateString());
    if (L610_GetCachedInfo(&info) == L610_OK)
    {
        status->net.rssi = info.rssi;
        strncpy(status->net.operator_name, info.operator_name, sizeof(status->net.operator_name) - 1U);
        strncpy(status->net.ip, info.ip_addr, sizeof(status->net.ip) - 1U);
    }
}

#if APP_MQTT_AUTO_RECONNECT_ENABLE
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
#endif

void App_MainInit(void)
{
    L610_Status_t sync_status;
    L610_SessionStatus_t session_status;

    Relay_Init();
    App_PrintBootStage("clock", "ok");
    App_PrintBootStage("fw", APP_FW_TAG);
    L610_Init();
    L610_MQTT_Init();
    Console_Init();
    AppWeather_Init();
    LCD_DebugUI_Init();
    App_PrintBootStage("lcd", (LCD_Port_IsReady() != 0U) ? "ok" : "fail");
    App_PrintBootStage("adc", g_weather_data.adc_mode_text);
    HAL_Delay(3000);
    sync_status = L610_ERROR;
    session_status = L610_BeginSession(L610_OWNER_DIAG);
    if (session_status == L610_SESSION_OK)
    {
        sync_status = L610_Sync(1U);
        L610_EndSession(L610_OWNER_DIAG);
    }
    App_PrintBootStage("l610 sync", App_GetL610StatusText(sync_status));
    App_L610Diag_Reset();
    if (sync_status == L610_OK)
    {
        app_l610_diag.at_ready = 1U;
        app_l610_diag.sim_ready = 1U;
    }
    App_PrintDiagBanner(sync_status);
#if APP_MQTT_AUTO_RECONNECT_ENABLE
    app_mqtt_last_retry_tick = 0U;
#endif
}

void App_MainTask(void)
{
#if APP_HEARTBEAT_LOG_ENABLE
    if (Console_IsBackgroundPauseActive() == 0U &&
        (HAL_GetTick() - app_last_heartbeat_tick) >= 1000U)
    {
        app_last_heartbeat_tick = HAL_GetTick();
        App_PrintHeartbeat();
    }
#endif

    Console_Task();
    L610_MQTT_Task();
    App_L610DiagTask();
    AppWeather_Task();
    LCD_DebugUI_Task();

    if (app_l610_diag.complete == 0U)
    {
        return;
    }

#if APP_MQTT_AUTO_RECONNECT_ENABLE
    if (L610_MQTT_GetState() >= MQTT_STATE_SUBSCRIBED)
    {
        app_mqtt_last_retry_tick = HAL_GetTick();
        return;
    }

    if (Console_IsBackgroundPauseActive() != 0U)
    {
        return;
    }

    if (app_mqtt_last_retry_tick == 0U ||
        (HAL_GetTick() - app_mqtt_last_retry_tick) >= APP_MQTT_RECONNECT_INTERVAL_MS)
    {
        app_mqtt_last_retry_tick = HAL_GetTick();
        App_MQTTBringUp();
    }
#else
    if (L610_MQTT_GetState() >= MQTT_STATE_SUBSCRIBED)
    {
        return;
    }
#endif
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
