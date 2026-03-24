#include "sensor_wind_zts.h"
#include "bsp_uart.h"
#include "board_rs485.h"
#include "modbus_rtu.h"
#include "weather_data.h"
#include "stm32h7xx_hal.h"
#include <stdio.h>
#include <string.h>

#define WIND_SENSOR_MAX_FAILURES         3U
#define WIND_SENSOR_QUERY_TIMEOUT_MS     1500U
#define WIND_SENSOR_UART_TIMEOUT_MS      200U
#define WIND_SENSOR_INTERFRAME_DELAY_MS  80U
#define WIND_SENSOR_ONLINE_HOLD_MS       3000U
#define WIND_RS485_MAX_FRAME_LEN         16U
#define WIND_RS485_RX_BUF_SIZE           48U
#define WIND_SENSOR_DEBUG_HEX_BYTES      12U

typedef struct
{
    uint8_t slave;
    uint8_t func_code;
    uint32_t baudrate;
    uint16_t reg_addr;
    uint16_t reg_count;
} WindModbusNodeConfig_t;

typedef struct
{
    const WindModbusNodeConfig_t *node;
    uint8_t waiting;
    uint8_t rx_buf[WIND_RS485_RX_BUF_SIZE];
    uint16_t rx_len;
    uint32_t started_tick;
    uint32_t next_send_tick;
} WindQueryState_t;

static const WindModbusNodeConfig_t s_wind_speed_node = {0x01U, 0x03U, 4800U, 0x0000U, 1U};
static const WindModbusNodeConfig_t s_wind_direction_node = {0x02U, 0x03U, 4800U, 0x0000U, 1U};

WindSensor_t g_wind_sensor = {0};
static WindQueryState_t s_query_state = {0};
static uint8_t s_next_query_is_direction = 0U;

static void WindSensor_SetFrameText(char *dst, uint16_t dst_len, const char *text)
{
    if (dst == NULL || dst_len == 0U)
    {
        return;
    }

    if (text == NULL)
    {
        text = "N/A";
    }

    strncpy(dst, text, dst_len - 1U);
    dst[dst_len - 1U] = '\0';
}

static void WindSensor_FormatFrameText(char *dst, uint16_t dst_len, const uint8_t *buf, uint16_t len)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    char hex_text[(WIND_SENSOR_DEBUG_HEX_BYTES * 2U) + 4U];
    uint16_t shown_len;
    uint16_t pos = 0U;
    uint16_t i;

    if (dst == NULL || dst_len == 0U)
    {
        return;
    }

    if (buf == NULL || len == 0U)
    {
        WindSensor_SetFrameText(dst, dst_len, "N/A");
        return;
    }

    shown_len = (len > WIND_SENSOR_DEBUG_HEX_BYTES) ? WIND_SENSOR_DEBUG_HEX_BYTES : len;
    for (i = 0U; i < shown_len && (pos + 2U) < sizeof(hex_text); i++)
    {
        hex_text[pos++] = hex_digits[(buf[i] >> 4) & 0x0FU];
        hex_text[pos++] = hex_digits[buf[i] & 0x0FU];
    }

    if ((len > shown_len) && (pos + 3U) < sizeof(hex_text))
    {
        hex_text[pos++] = '.';
        hex_text[pos++] = '.';
        hex_text[pos++] = '.';
    }
    hex_text[pos] = '\0';

    (void)snprintf(dst, dst_len, "%uB %s", (unsigned)len, hex_text);
}

static void WindSensor_SetDirectionText(const char *text)
{
    if (text == NULL)
    {
        text = "N/A";
    }

    strncpy(g_wind_sensor.direction_text, text, sizeof(g_wind_sensor.direction_text) - 1U);
    g_wind_sensor.direction_text[sizeof(g_wind_sensor.direction_text) - 1U] = '\0';
}

static void WindSensor_ResetSpeedValue(void)
{
    g_wind_sensor.speed_raw_reg = 0U;
    g_wind_sensor.wind_speed_mps = 0.0f;
    g_wind_sensor.speed_last_ok_tick = 0U;
}

static void WindSensor_ResetDirectionValue(void)
{
    g_wind_sensor.direction_raw_reg = 0U;
    g_wind_sensor.wind_dir_deg = 0.0f;
    g_wind_sensor.direction_last_ok_tick = 0U;
    WindSensor_SetDirectionText("N/A");
}

static void WindSensor_PublishSnapshot(uint8_t online)
{
    g_weather_data.wind_speed_raw = g_wind_sensor.speed_raw_reg;
    g_weather_data.wind_direction_raw = g_wind_sensor.direction_raw_reg;
    g_weather_data.wind_speed_mps = g_wind_sensor.wind_speed_mps;
    g_weather_data.wind_dir_deg = g_wind_sensor.wind_dir_deg;
    g_weather_data.wind_speed_voltage = 0.0f;
    g_weather_data.wind_direction_voltage = 0.0f;
    strncpy(g_weather_data.wind_direction_text,
            g_wind_sensor.direction_text,
            sizeof(g_weather_data.wind_direction_text) - 1U);
    g_weather_data.wind_direction_text[sizeof(g_weather_data.wind_direction_text) - 1U] = '\0';
    g_weather_data.wind_online = online;
    g_wind_sensor.base.online = online;
}

static uint8_t WindSensor_IsDataFresh(uint32_t now, uint32_t last_ok_tick)
{
    if (last_ok_tick == 0U)
    {
        return 0U;
    }

    return (uint8_t)(((now - last_ok_tick) <= WIND_SENSOR_ONLINE_HOLD_MS) ? 1U : 0U);
}

static HAL_StatusTypeDef WindSensor_SetUartBaudrate(uint32_t baudrate)
{
    if (g_uart_wind.huart == NULL)
    {
        return HAL_ERROR;
    }

    if (g_uart_wind.huart->Init.BaudRate == baudrate)
    {
        return HAL_OK;
    }

    return HAL_ERROR;
}

static void WindSensor_AppendRxByte(uint8_t byte)
{
    if (s_query_state.rx_len < sizeof(s_query_state.rx_buf))
    {
        s_query_state.rx_buf[s_query_state.rx_len++] = byte;
        return;
    }

    memmove(s_query_state.rx_buf, s_query_state.rx_buf + 1, sizeof(s_query_state.rx_buf) - 1U);
    s_query_state.rx_buf[sizeof(s_query_state.rx_buf) - 1U] = byte;
    s_query_state.rx_len = (uint16_t)sizeof(s_query_state.rx_buf);
}

static void WindSensor_ReadIncoming(void)
{
    uint8_t tmp[16];
    uint16_t available;
    uint16_t to_read;
    uint16_t i;

    available = UartRingBuf_Available(&g_uart_wind.rx_rb);
    if (available == 0U)
    {
        return;
    }

    to_read = (available > sizeof(tmp)) ? (uint16_t)sizeof(tmp) : available;
    to_read = UartRingBuf_Read(&g_uart_wind.rx_rb, tmp, to_read);
    for (i = 0U; i < to_read; i++)
    {
        WindSensor_AppendRxByte(tmp[i]);
    }

    WindSensor_FormatFrameText(g_wind_sensor.last_rx_hex,
                               (uint16_t)sizeof(g_wind_sensor.last_rx_hex),
                               s_query_state.rx_buf,
                               s_query_state.rx_len);
}

static uint8_t WindSensor_TryParseFrame(uint16_t *out_regs)
{
    uint16_t expected_len;
    uint16_t start;

    if (s_query_state.node == NULL || out_regs == NULL)
    {
        return 0U;
    }

    expected_len = (uint16_t)(5U + (s_query_state.node->reg_count * 2U));
    if (s_query_state.rx_len < expected_len)
    {
        return 0U;
    }

    for (start = 0U; (uint16_t)(start + expected_len) <= s_query_state.rx_len; start++)
    {
        if (Modbus_ParseReadRegsResp(&s_query_state.rx_buf[start],
                                     expected_len,
                                     s_query_state.node->slave,
                                     s_query_state.node->func_code,
                                     s_query_state.node->reg_count,
                                     out_regs) != 0U)
        {
            return 1U;
        }
    }

    return 0U;
}

static const WindModbusNodeConfig_t *WindSensor_GetActiveNode(void)
{
    return (s_next_query_is_direction != 0U) ? &s_wind_direction_node : &s_wind_speed_node;
}

static void WindSensor_DecodeDirectionText(float direction_deg)
{
    static const char *const direction_names[8] = {
        "N", "NE", "E", "SE", "S", "SW", "W", "NW"
    };
    float normalized = direction_deg;
    int index;

    while (normalized < 0.0f)
    {
        normalized += 360.0f;
    }
    while (normalized >= 360.0f)
    {
        normalized -= 360.0f;
    }

    index = (int)((normalized + 22.5f) / 45.0f);
    index %= 8;
    WindSensor_SetDirectionText(direction_names[index]);
}

static void WindSensor_CompleteQuery(uint8_t success, uint16_t raw_value, uint32_t now)
{
    uint8_t rx_seen = (uint8_t)((s_query_state.rx_len > 0U) ? 1U : 0U);

    if (g_wind_sensor.current_query_is_direction == 0U)
    {
        if (success != 0U)
        {
            g_wind_sensor.speed_failures = 0U;
            g_wind_sensor.speed_raw_reg = raw_value;
            g_wind_sensor.wind_speed_mps = ((float)raw_value) / 10.0f;
            g_wind_sensor.speed_last_ok_tick = now;
            g_wind_sensor.last_query_raw = raw_value;
            g_wind_sensor.last_query_failures = 0U;
        }
        else
        {
            if (g_wind_sensor.speed_failures < 0xFFU)
            {
                g_wind_sensor.speed_failures++;
            }
            g_wind_sensor.last_query_raw = g_wind_sensor.speed_raw_reg;
            g_wind_sensor.last_query_failures = g_wind_sensor.speed_failures;
            if (g_wind_sensor.speed_failures >= WIND_SENSOR_MAX_FAILURES)
            {
                WindSensor_ResetSpeedValue();
            }
        }
    }
    else
    {
        if (success != 0U)
        {
            g_wind_sensor.direction_failures = 0U;
            g_wind_sensor.direction_raw_reg = raw_value;
            g_wind_sensor.wind_dir_deg = ((float)raw_value) / 10.0f;
            g_wind_sensor.direction_last_ok_tick = now;
            WindSensor_DecodeDirectionText(g_wind_sensor.wind_dir_deg);
            g_wind_sensor.last_query_raw = raw_value;
            g_wind_sensor.last_query_failures = 0U;
        }
        else
        {
            if (g_wind_sensor.direction_failures < 0xFFU)
            {
                g_wind_sensor.direction_failures++;
            }
            g_wind_sensor.last_query_raw = g_wind_sensor.direction_raw_reg;
            g_wind_sensor.last_query_failures = g_wind_sensor.direction_failures;
            if (g_wind_sensor.direction_failures >= WIND_SENSOR_MAX_FAILURES)
            {
                WindSensor_ResetDirectionValue();
            }
        }
    }

    s_query_state.waiting = 0U;
    s_query_state.node = NULL;
    s_query_state.rx_len = 0U;
    if ((success == 0U) && (rx_seen == 0U))
    {
        WindSensor_SetFrameText(g_wind_sensor.last_rx_hex,
                                (uint16_t)sizeof(g_wind_sensor.last_rx_hex),
                                "TIMEOUT");
    }
    s_query_state.next_send_tick = now + WIND_SENSOR_INTERFRAME_DELAY_MS;
    s_next_query_is_direction ^= 1U;
}

static void WindSensor_StartQuery(uint32_t now)
{
    uint8_t tx_buf[8];
    uint16_t tx_len;

    g_wind_sensor.current_query_is_direction = s_next_query_is_direction;
    s_query_state.node = WindSensor_GetActiveNode();
    s_query_state.rx_len = 0U;
    UartRingBuf_Clear(&g_uart_wind.rx_rb);

    if (WindSensor_SetUartBaudrate(s_query_state.node->baudrate) != HAL_OK)
    {
        WindSensor_CompleteQuery(0U, 0U, now);
        return;
    }

    tx_len = Modbus_BuildReadRegs(s_query_state.node->slave,
                                  s_query_state.node->func_code,
                                  s_query_state.node->reg_addr,
                                  s_query_state.node->reg_count,
                                  tx_buf);
    WindSensor_FormatFrameText(g_wind_sensor.last_tx_hex,
                               (uint16_t)sizeof(g_wind_sensor.last_tx_hex),
                               tx_buf,
                               tx_len);
    WindSensor_SetFrameText(g_wind_sensor.last_rx_hex,
                            (uint16_t)sizeof(g_wind_sensor.last_rx_hex),
                            "N/A");

    if (BoardRS485_Transmit(g_uart_wind.huart, tx_buf, tx_len, WIND_SENSOR_UART_TIMEOUT_MS) != HAL_OK)
    {
        WindSensor_CompleteQuery(0U, 0U, now);
        return;
    }

    s_query_state.waiting = 1U;
    s_query_state.started_tick = now;
}

void WindSensor_Init(void)
{
    memset(&g_wind_sensor, 0, sizeof(g_wind_sensor));
    memset(&s_query_state, 0, sizeof(s_query_state));
    g_wind_sensor.base.last_update_tick = 0U;
    g_wind_sensor.last_poll_tick = 0U;
    s_next_query_is_direction = 0U;
    WindSensor_ResetSpeedValue();
    WindSensor_ResetDirectionValue();
    g_wind_sensor.last_query_raw = 0U;
    g_wind_sensor.last_query_failures = 0U;
    g_wind_sensor.current_query_is_direction = 0U;
    WindSensor_SetFrameText(g_wind_sensor.last_tx_hex,
                            (uint16_t)sizeof(g_wind_sensor.last_tx_hex),
                            "N/A");
    WindSensor_SetFrameText(g_wind_sensor.last_rx_hex,
                            (uint16_t)sizeof(g_wind_sensor.last_rx_hex),
                            "N/A");
    WindSensor_PublishSnapshot(0U);
}

void WindSensor_Poll(void)
{
    uint16_t regs[1] = {0U};
    uint8_t overall_online;
    uint32_t now = HAL_GetTick();

    g_wind_sensor.last_poll_tick = now;
    WindSensor_ReadIncoming();

    if (s_query_state.waiting != 0U)
    {
        if (WindSensor_TryParseFrame(regs) != 0U)
        {
            WindSensor_CompleteQuery(1U, regs[0], now);
        }
        else if ((now - s_query_state.started_tick) >= WIND_SENSOR_QUERY_TIMEOUT_MS)
        {
            WindSensor_CompleteQuery(0U, 0U, now);
        }
    }
    else if ((s_query_state.next_send_tick == 0U) || (now >= s_query_state.next_send_tick))
    {
        WindSensor_StartQuery(now);
    }

    overall_online = (uint8_t)((WindSensor_IsDataFresh(now, g_wind_sensor.speed_last_ok_tick) != 0U) &&
                               (WindSensor_IsDataFresh(now, g_wind_sensor.direction_last_ok_tick) != 0U));
    if (overall_online != 0U)
    {
        g_wind_sensor.base.last_update_tick = now;
    }

    WindSensor_PublishSnapshot(overall_online);
}

void WindSensor_Parse(void)
{
    /* Wind sensors use non-blocking Modbus polling; no async parser is needed. */
}
