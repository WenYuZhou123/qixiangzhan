#include "panel_link.h"

#include "board_rs485.h"
#include "bsp_uart.h"
#include "modbus_rtu.h"
#include "sensor_wind_zts.h"
#include "weather_data.h"
#include "relay.h"
#include "protocol.h"
#include "stm32h7xx_hal.h"

#include <string.h>

#define PANEL_LINK_ADDRESS                0x7EU
#define PANEL_LINK_FUNCTION               0x41U
#define PANEL_LINK_OP_POLL                0x01U
#define PANEL_LINK_OP_PING                0x02U
#define PANEL_LINK_OP_STATUS              0x03U
#define PANEL_LINK_OP_READY               0x81U
#define PANEL_LINK_OP_ACK                 0x82U
#define PANEL_LINK_OP_ERROR               0xFFU
#define PANEL_LINK_FLAG_PING_REQUEST      0x01U
#define PANEL_LINK_STATUS_VERSION         1U
#define PANEL_LINK_STATUS_PAYLOAD_SIZE    34U
#define PANEL_LINK_FLAG_RELAY1_ON         0x01U
#define PANEL_LINK_FLAG_RELAY2_ON         0x02U
#define PANEL_LINK_FLAG_WIND_ONLINE       0x04U
#define PANEL_LINK_FLAG_AIR_ONLINE        0x08U
#define PANEL_LINK_FLAG_RAIN_ONLINE       0x10U
#define PANEL_LINK_FLAG_RAIN_DETECTED     0x20U
#define PANEL_LINK_COMMAND_NONE           0U
#define PANEL_LINK_COMMAND_PAD_OPEN       1U
#define PANEL_LINK_COMMAND_PAD_CLOSE      2U
#define PANEL_LINK_COMMAND_PAD_STOP       3U
#define PANEL_LINK_COMMAND_QUERY_STATUS   4U
#define PANEL_LINK_COMMAND_RESULT_NONE    0U
#define PANEL_LINK_COMMAND_RESULT_SUCCESS 1U
#define PANEL_LINK_COMMAND_RESULT_ERROR   2U
#define PANEL_LINK_MAX_PAYLOAD            48U
#define PANEL_LINK_MAX_FRAME              (7U + PANEL_LINK_MAX_PAYLOAD)
#define PANEL_LINK_POLL_INTERVAL_MS       1000U
#define PANEL_LINK_RESPONSE_TIMEOUT_MS    250U
#define PANEL_LINK_INTERFRAME_DELAY_MS    30U
#define PANEL_LINK_UART_TIMEOUT_MS        100U

typedef struct
{
    PanelLink_Status_t status;
    uint8_t next_sequence;
    uint8_t expected_operation;
    uint8_t ping_pending;
    uint32_t request_started_tick;
    uint32_t next_request_tick;
    uint8_t rx_buffer[PANEL_LINK_MAX_FRAME * 2U];
    uint16_t rx_length;
} PanelLink_Context_t;

static PanelLink_Context_t s_panel_link;

static uint16_t PanelLink_EncodeU16(float value, float scale)
{
    float scaled = value * scale;

    if (scaled <= 0.0f)
    {
        return 0U;
    }
    if (scaled >= 65535.0f)
    {
        return 65535U;
    }
    return (uint16_t)(scaled + 0.5f);
}

static int16_t PanelLink_EncodeI16(float value, float scale)
{
    float scaled = value * scale;

    if (scaled <= -32768.0f)
    {
        return -32768;
    }
    if (scaled >= 32767.0f)
    {
        return 32767;
    }
    return (int16_t)((scaled < 0.0f) ? (scaled - 0.5f) : (scaled + 0.5f));
}

static void PanelLink_WriteU16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)(value >> 8U);
}

static void PanelLink_WriteU32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8U) & 0xFFU);
    dst[2] = (uint8_t)((value >> 16U) & 0xFFU);
    dst[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

static void PanelLink_BuildStatusPayload(uint8_t payload[PANEL_LINK_STATUS_PAYLOAD_SIZE])
{
    uint8_t flags = 0U;

    memset(payload, 0, PANEL_LINK_STATUS_PAYLOAD_SIZE);
    if (Relay_GetState(RELAY1) == RELAY_ON) flags |= PANEL_LINK_FLAG_RELAY1_ON;
    if (Relay_GetState(RELAY2) == RELAY_ON) flags |= PANEL_LINK_FLAG_RELAY2_ON;
    if (g_weather_data.wind_online != 0U) flags |= PANEL_LINK_FLAG_WIND_ONLINE;
    if (g_weather_data.air_online != 0U) flags |= PANEL_LINK_FLAG_AIR_ONLINE;
    if (g_weather_data.rain_online != 0U) flags |= PANEL_LINK_FLAG_RAIN_ONLINE;
    if (g_weather_data.rain_detected != 0U) flags |= PANEL_LINK_FLAG_RAIN_DETECTED;

    payload[0] = PANEL_LINK_STATUS_VERSION;
    payload[1] = flags;
    payload[2] = s_panel_link.status.last_command;
    payload[3] = s_panel_link.status.last_command_result;
    PanelLink_WriteU32(&payload[4], HAL_GetTick());
    PanelLink_WriteU16(&payload[8], PanelLink_EncodeU16(g_weather_data.wind_speed_mps, 10.0f));
    PanelLink_WriteU16(&payload[10], PanelLink_EncodeU16(g_weather_data.wind_dir_deg, 10.0f));
    PanelLink_WriteU16(&payload[12], (uint16_t)PanelLink_EncodeI16(g_weather_data.temperature_c, 100.0f));
    PanelLink_WriteU16(&payload[14], PanelLink_EncodeU16(g_weather_data.humidity_rh, 10.0f));
    PanelLink_WriteU16(&payload[16], PanelLink_EncodeU16(g_weather_data.rain_value, 10.0f));
    PanelLink_WriteU16(&payload[18], PanelLink_EncodeU16(g_weather_data.pm25_ugm3, 10.0f));
    PanelLink_WriteU16(&payload[20], PanelLink_EncodeU16(g_weather_data.pm10_ugm3, 10.0f));
    PanelLink_WriteU16(&payload[22], PanelLink_EncodeU16(g_weather_data.co2_ppm, 1.0f));
    PanelLink_WriteU16(&payload[24], PanelLink_EncodeU16(g_weather_data.tvoc_mg_m3, 1000.0f));
    PanelLink_WriteU16(&payload[26], PanelLink_EncodeU16(g_weather_data.ch2o_mg_m3, 1000.0f));
    PanelLink_WriteU16(&payload[28], g_weather_data.wind_speed_raw);
    PanelLink_WriteU16(&payload[30], g_weather_data.wind_direction_raw);
    PanelLink_WriteU16(&payload[32], g_weather_data.rain_adc_raw);
}

static uint16_t PanelLink_Crc16(const uint8_t *data, uint16_t len)
{
    return Modbus_CRC16(data, len);
}

static void PanelLink_ResetRx(void)
{
    s_panel_link.rx_length = 0U;
    memset(s_panel_link.rx_buffer, 0, sizeof(s_panel_link.rx_buffer));
}

static uint8_t PanelLink_SendRequest(uint8_t operation,
                                     const uint8_t *payload,
                                     uint8_t payload_len,
                                     uint32_t now)
{
    uint8_t frame[PANEL_LINK_MAX_FRAME];
    uint16_t crc;
    uint16_t length = 0U;

    if (g_uart_wind.huart == NULL)
    {
        return 0U;
    }

    frame[length++] = PANEL_LINK_ADDRESS;
    frame[length++] = PANEL_LINK_FUNCTION;
    frame[length++] = s_panel_link.next_sequence++;
    frame[length++] = operation;
    if (payload_len > PANEL_LINK_MAX_PAYLOAD)
    {
        return 0U;
    }
    frame[length++] = payload_len;
    if (payload_len > 0U && payload != NULL)
    {
        memcpy(&frame[length], payload, payload_len);
        length = (uint16_t)(length + payload_len);
    }
    crc = PanelLink_Crc16(frame, length);
    frame[length++] = (uint8_t)(crc & 0xFFU);
    frame[length++] = (uint8_t)(crc >> 8U);

    UartRingBuf_Clear(&g_uart_wind.rx_rb);
    PanelLink_ResetRx();
    if (BoardRS485_Transmit(g_uart_wind.huart, frame, length, PANEL_LINK_UART_TIMEOUT_MS) != HAL_OK)
    {
        return 0U;
    }

    s_panel_link.status.waiting = 1U;
    s_panel_link.status.last_sequence = frame[2];
    s_panel_link.status.last_operation = operation;
    s_panel_link.expected_operation = (operation == PANEL_LINK_OP_PING) ? PANEL_LINK_OP_ACK : PANEL_LINK_OP_READY;
    s_panel_link.request_started_tick = now;
    s_panel_link.status.tx_frames++;
    return 1U;
}

static uint8_t PanelLink_SendStatus(uint32_t now)
{
    uint8_t payload[PANEL_LINK_STATUS_PAYLOAD_SIZE];

    PanelLink_BuildStatusPayload(payload);
    return PanelLink_SendRequest(PANEL_LINK_OP_STATUS,
                                 payload,
                                 sizeof(payload),
                                 now);
}

static uint8_t PanelLink_CommandReachedTarget(uint8_t command)
{
    uint8_t relay1_on = (Relay_GetState(RELAY1) == RELAY_ON) ? 1U : 0U;
    uint8_t relay2_on = (Relay_GetState(RELAY2) == RELAY_ON) ? 1U : 0U;

    switch (command)
    {
    case PANEL_LINK_COMMAND_PAD_OPEN:
        return (uint8_t)((relay1_on != 0U && relay2_on != 0U) ? 1U : 0U);
    case PANEL_LINK_COMMAND_PAD_CLOSE:
    case PANEL_LINK_COMMAND_PAD_STOP:
        return (uint8_t)((relay1_on == 0U && relay2_on == 0U) ? 1U : 0U);
    case PANEL_LINK_COMMAND_QUERY_STATUS:
        return 1U;
    default:
        return 0U;
    }
}

static void PanelLink_HandleCommand(uint8_t command)
{
    ProtocolStatus_t status = PROTOCOL_OK;
    char command_text[24];

    command_text[0] = '\0';

    switch (command)
    {
    case PANEL_LINK_COMMAND_NONE:
        return;
    case PANEL_LINK_COMMAND_PAD_OPEN:
        strcpy(command_text, "PAD_OPEN");
        break;
    case PANEL_LINK_COMMAND_PAD_CLOSE:
        strcpy(command_text, "PAD_CLOSE");
        break;
    case PANEL_LINK_COMMAND_PAD_STOP:
        strcpy(command_text, "PAD_STOP");
        break;
    case PANEL_LINK_COMMAND_QUERY_STATUS:
        strcpy(command_text, "QUERY_PAD_STATUS");
        break;
    default:
        s_panel_link.status.last_command = command;
        s_panel_link.status.last_command_result = PANEL_LINK_COMMAND_RESULT_ERROR;
        return;
    }

    /* Protocol_Parse tokenizes its input, so it must never receive a string literal. */
    status = Protocol_Parse(command_text);
    s_panel_link.status.last_command = command;
    s_panel_link.status.last_command_result = (status == PROTOCOL_OK &&
                                                PanelLink_CommandReachedTarget(command) != 0U) ?
                                                PANEL_LINK_COMMAND_RESULT_SUCCESS :
                                                PANEL_LINK_COMMAND_RESULT_ERROR;
}

static void PanelLink_AppendRxByte(uint8_t byte)
{
    if (s_panel_link.rx_length < sizeof(s_panel_link.rx_buffer))
    {
        s_panel_link.rx_buffer[s_panel_link.rx_length++] = byte;
        return;
    }

    memmove(s_panel_link.rx_buffer,
            s_panel_link.rx_buffer + 1U,
            sizeof(s_panel_link.rx_buffer) - 1U);
    s_panel_link.rx_buffer[sizeof(s_panel_link.rx_buffer) - 1U] = byte;
}

static void PanelLink_ReadIncoming(void)
{
    uint8_t data[16];
    uint16_t available;
    uint16_t count;
    uint16_t index;

    available = UartRingBuf_Available(&g_uart_wind.rx_rb);
    while (available > 0U)
    {
        count = (available > sizeof(data)) ? (uint16_t)sizeof(data) : available;
        count = UartRingBuf_Read(&g_uart_wind.rx_rb, data, count);
        for (index = 0U; index < count; index++)
        {
            PanelLink_AppendRxByte(data[index]);
        }
        available = UartRingBuf_Available(&g_uart_wind.rx_rb);
    }
}

static uint8_t PanelLink_TryParseResponse(uint8_t *payload, uint8_t *payload_len)
{
    uint16_t start;

    if (payload == NULL || payload_len == NULL)
    {
        return 0U;
    }

    for (start = 0U; (uint16_t)(start + 7U) <= s_panel_link.rx_length; start++)
    {
        uint16_t frame_len;
        uint16_t crc_calculated;
        uint16_t crc_received;
        uint8_t length;

        if (s_panel_link.rx_buffer[start] != PANEL_LINK_ADDRESS ||
            s_panel_link.rx_buffer[start + 1U] != PANEL_LINK_FUNCTION ||
            s_panel_link.rx_buffer[start + 2U] != s_panel_link.status.last_sequence ||
            s_panel_link.rx_buffer[start + 3U] != s_panel_link.expected_operation)
        {
            continue;
        }

        length = s_panel_link.rx_buffer[start + 4U];
        if (length > PANEL_LINK_MAX_PAYLOAD)
        {
            continue;
        }

        frame_len = (uint16_t)(7U + length);
        if ((uint16_t)(start + frame_len) > s_panel_link.rx_length)
        {
            return 0U;
        }

        crc_calculated = PanelLink_Crc16(&s_panel_link.rx_buffer[start], frame_len - 2U);
        crc_received = (uint16_t)s_panel_link.rx_buffer[start + frame_len - 2U] |
                       (uint16_t)((uint16_t)s_panel_link.rx_buffer[start + frame_len - 1U] << 8U);
        if (crc_calculated != crc_received)
        {
            s_panel_link.status.crc_errors++;
            continue;
        }

        if (length > 0U)
        {
            memcpy(payload, &s_panel_link.rx_buffer[start + 5U], length);
        }
        *payload_len = length;
        return 1U;
    }

    return 0U;
}

void PanelLink_Init(void)
{
    memset(&s_panel_link, 0, sizeof(s_panel_link));
    s_panel_link.next_request_tick = HAL_GetTick() + PANEL_LINK_INTERFRAME_DELAY_MS;
}

uint8_t PanelLink_IsTransactionActive(void)
{
    return s_panel_link.status.waiting;
}

void PanelLink_GetStatus(PanelLink_Status_t *status)
{
    if (status != NULL)
    {
        *status = s_panel_link.status;
    }
}

void PanelLink_Task(void)
{
    uint8_t payload[PANEL_LINK_MAX_PAYLOAD];
    uint8_t payload_len = 0U;
    uint32_t now = HAL_GetTick();

    if (s_panel_link.status.waiting != 0U)
    {
        PanelLink_ReadIncoming();
        if (PanelLink_TryParseResponse(payload, &payload_len) != 0U)
        {
            s_panel_link.status.waiting = 0U;
            s_panel_link.status.online = 1U;
            s_panel_link.status.rx_frames++;
            s_panel_link.status.last_response_tick = now;
            if (s_panel_link.expected_operation == PANEL_LINK_OP_READY &&
                payload_len > 0U &&
                (payload[0] & PANEL_LINK_FLAG_PING_REQUEST) != 0U)
            {
                s_panel_link.ping_pending = 1U;
                s_panel_link.next_request_tick = now + PANEL_LINK_INTERFRAME_DELAY_MS;
            }
            else
            {
                s_panel_link.next_request_tick = now + PANEL_LINK_POLL_INTERVAL_MS;
            }
            if (s_panel_link.expected_operation == PANEL_LINK_OP_READY && payload_len > 1U)
            {
                PanelLink_HandleCommand(payload[1]);
                s_panel_link.next_request_tick = now + PANEL_LINK_INTERFRAME_DELAY_MS;
            }
        }
        else if ((now - s_panel_link.request_started_tick) >= PANEL_LINK_RESPONSE_TIMEOUT_MS)
        {
            s_panel_link.status.waiting = 0U;
            s_panel_link.status.online = 0U;
            s_panel_link.status.timeout_count++;
            s_panel_link.next_request_tick = now + PANEL_LINK_POLL_INTERVAL_MS;
        }
        return;
    }

    if (WindSensor_IsBusy() != 0U || now < s_panel_link.next_request_tick)
    {
        return;
    }

    if (s_panel_link.ping_pending != 0U)
    {
        if (PanelLink_SendRequest(PANEL_LINK_OP_PING, NULL, 0U, now) != 0U)
        {
            s_panel_link.ping_pending = 0U;
        }
        return;
    }

    (void)PanelLink_SendStatus(now);
}
