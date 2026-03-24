#include "console.h"
#include "protocol.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>

#define CONSOLE_LOCAL_ECHO 0
#define CONSOLE_BACKGROUND_PAUSE_MS 30000U
#define CONSOLE_CMD_IDLE_EXEC_MS 800U
#define CONSOLE_RX_RING_SIZE 256U

static UART_HandleTypeDef *s_console_uart = NULL;
static uint8_t s_console_rx_it_byte = 0U;
static uint8_t s_console_rx_ring[CONSOLE_RX_RING_SIZE];
static volatile uint16_t s_console_rx_head = 0U;
static volatile uint16_t s_console_rx_tail = 0U;
static char cmd_buf[256];
static uint8_t cmd_idx = 0U;
static uint32_t s_console_pause_until_tick = 0U;
static uint32_t s_console_last_rx_tick = 0U;

static void Console_ProcessPendingCommand(void);
static void Console_StartRecvIT(void);

static void Console_Write(const char *text)
{
    if (s_console_uart != NULL && text != NULL && text[0] != '\0')
    {
        HAL_UART_Transmit(s_console_uart, (uint8_t *)text, (uint16_t)strlen(text), 1000);
    }
}

static void Console_RequestBackgroundPause(void)
{
    s_console_pause_until_tick = HAL_GetTick() + CONSOLE_BACKGROUND_PAUSE_MS;
}

static void Console_ClearRxRing(void)
{
    s_console_rx_head = 0U;
    s_console_rx_tail = 0U;
}

static void Console_ClearCommandBuffer(void)
{
    memset(cmd_buf, 0, sizeof(cmd_buf));
    cmd_idx = 0U;
    s_console_last_rx_tick = 0U;
}

static void Console_StartRecvIT(void)
{
    if (s_console_uart != NULL)
    {
        (void)HAL_UART_Receive_IT(s_console_uart, &s_console_rx_it_byte, 1);
    }
}

static void Console_PushRxByte(uint8_t ch)
{
    uint16_t next_head;

    next_head = (uint16_t)((s_console_rx_head + 1U) % CONSOLE_RX_RING_SIZE);
    if (next_head == s_console_rx_tail)
    {
        s_console_rx_tail = (uint16_t)((s_console_rx_tail + 1U) % CONSOLE_RX_RING_SIZE);
    }

    s_console_rx_ring[s_console_rx_head] = ch;
    s_console_rx_head = next_head;
}

static uint8_t Console_PopRxByte(uint8_t *out_ch)
{
    if (out_ch == NULL || s_console_rx_tail == s_console_rx_head)
    {
        return 0U;
    }

    *out_ch = s_console_rx_ring[s_console_rx_tail];
    s_console_rx_tail = (uint16_t)((s_console_rx_tail + 1U) % CONSOLE_RX_RING_SIZE);
    return 1U;
}

static void Console_HandleRxByte(uint8_t ch)
{
    Console_RequestBackgroundPause();
    s_console_last_rx_tick = HAL_GetTick();

#if CONSOLE_LOCAL_ECHO
    HAL_UART_Transmit(s_console_uart, &ch, 1, 1000);
#endif

    if (ch == '\r' || ch == '\n')
    {
        if (cmd_idx > 0U)
        {
            Console_ProcessPendingCommand();
        }
        return;
    }

    if (cmd_idx < sizeof(cmd_buf) - 1U)
    {
        cmd_buf[cmd_idx++] = (char)ch;
    }
    else
    {
        Console_ClearCommandBuffer();
        Console_Write("\r\nCMD TOO LONG\r\n> ");
    }
}

static void Console_ProcessPendingCommand(void)
{
    const char *response;
    const char *status_text;
    uint16_t response_len;
    ProtocolStatus_t status;

    if (cmd_idx == 0U)
    {
        return;
    }

    cmd_buf[cmd_idx] = '\0';

    Console_Write("\r\nCMD: ");
    Console_Write(cmd_buf);
    Console_Write("\r\n");

    Console_RequestBackgroundPause();
    status = Protocol_Parse(cmd_buf);
    response = Protocol_GetLastResponse();
    status_text = Protocol_GetLastStatusText();

    Console_Write("STATUS: ");
    if (status_text != NULL && status_text[0] != '\0')
    {
        Console_Write(status_text);
    }
    else if (status == PROTOCOL_UNKNOWN_CMD)
    {
        Console_Write("UNKNOWN_CMD");
    }
    else
    {
        Console_Write("ERROR");
    }
    Console_Write("\r\n");

    if (status == PROTOCOL_OK ||
        ((status == PROTOCOL_INVALID_PARAM || status == PROTOCOL_EXEC_ERROR) &&
         response != NULL && response[0] != '\0'))
    {
        Console_Write("RX:\r\n");
        if (response != NULL && response[0] != '\0')
        {
            response_len = (uint16_t)strlen(response);
            HAL_UART_Transmit(s_console_uart, (uint8_t *)response, response_len, 1000);
            if (response_len < 1U || response[response_len - 1U] != '\n')
            {
                Console_Write("\r\n");
            }
        }
    }
    else
    {
        Console_Write("RX:\r\nUNKNOWN CMD\r\n");
    }

    Console_ClearCommandBuffer();
    Console_Write("> ");
}

void Console_Init(void)
{
    s_console_uart = &huart6;
    s_console_rx_it_byte = 0U;
    s_console_pause_until_tick = 0U;
    Console_ClearRxRing();
    Console_ClearCommandBuffer();
    Console_StartRecvIT();
    Console_Write("\r\n[USART6] debug console ready @115200 (mqtt-mipcall-fix-uart6-v1)\r\n");
    Console_Write("Commands: ATRAW, SELFTEST, NETINIT, MQTTINIT, MQTTCLOSE, STATUS\r\n> ");
}

uint8_t Console_IsBackgroundPauseActive(void)
{
    if (s_console_pause_until_tick == 0U)
    {
        return 0U;
    }

    return ((int32_t)(HAL_GetTick() - s_console_pause_until_tick) < 0) ? 1U : 0U;
}

void Console_Task(void)
{
    uint8_t ch;
    uint8_t got_any;

    if (s_console_uart == NULL)
    {
        return;
    }

    got_any = 0U;
    while (Console_PopRxByte(&ch) != 0U)
    {
        got_any = 1U;
        Console_HandleRxByte(ch);
    }

    if (got_any == 0U &&
        cmd_idx > 0U &&
        s_console_last_rx_tick != 0U &&
        (HAL_GetTick() - s_console_last_rx_tick) >= CONSOLE_CMD_IDLE_EXEC_MS)
    {
        Console_ProcessPendingCommand();
    }
}

void Console_OnUartRxCplt(UART_HandleTypeDef *huart)
{
    if (huart != s_console_uart)
    {
        return;
    }

    Console_PushRxByte(s_console_rx_it_byte);
    Console_StartRecvIT();
}

void Console_OnUartError(UART_HandleTypeDef *huart)
{
    if (huart != s_console_uart)
    {
        return;
    }

    __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_PEF | UART_CLEAR_FEF);
    (void)HAL_UART_AbortReceive(huart);
    Console_ClearRxRing();
    Console_ClearCommandBuffer();
    Console_StartRecvIT();
}
