#include "console.h"
#include "protocol.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>

#define CONSOLE_LOCAL_ECHO 0
#define CONSOLE_RX_TIMEOUT_MS 0

static uint8_t rx_ch;
static char cmd_buf[256];
static uint8_t cmd_idx = 0;

void Console_Task(void)
{
    const char *response;
    const char *status_text;
    uint16_t response_len;
    ProtocolStatus_t status;

    if (HAL_UART_Receive(&huart2, &rx_ch, 1, CONSOLE_RX_TIMEOUT_MS) == HAL_OK)
    {
        /* 回显输入字符，可选 */
#if CONSOLE_LOCAL_ECHO
        HAL_UART_Transmit(&huart2, &rx_ch, 1, 1000);
#endif

        if (rx_ch == '\r' || rx_ch == '\n')
        {
            if (cmd_idx > 0)
            {
                cmd_buf[cmd_idx] = '\0';

                HAL_UART_Transmit(&huart2, (uint8_t *)"\r\nCMD: ", 7, 1000);
                HAL_UART_Transmit(&huart2, (uint8_t *)cmd_buf, strlen(cmd_buf), 1000);
                HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, 1000);

                status = Protocol_Parse(cmd_buf);
                response = Protocol_GetLastResponse();
                status_text = Protocol_GetLastStatusText();
                HAL_UART_Transmit(&huart2, (uint8_t *)"STATUS: ", 8, 1000);
                if (status_text != NULL && status_text[0] != '\0')
                {
                    HAL_UART_Transmit(&huart2, (uint8_t *)status_text, strlen(status_text), 1000);
                }
                else if (status == PROTOCOL_UNKNOWN_CMD)
                {
                    HAL_UART_Transmit(&huart2, (uint8_t *)"UNKNOWN_CMD", 11, 1000);
                }
                else
                {
                    HAL_UART_Transmit(&huart2, (uint8_t *)"ERROR", 5, 1000);
                }
                HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, 1000);

                if (status == PROTOCOL_OK ||
                    ((status == PROTOCOL_INVALID_PARAM || status == PROTOCOL_EXEC_ERROR) &&
                     response != NULL && response[0] != '\0'))
                {
                    HAL_UART_Transmit(&huart2, (uint8_t *)"RX:\r\n", 5, 1000);
                    if (response != NULL && response[0] != '\0')
                    {
                        response_len = (uint16_t)strlen(response);
                        HAL_UART_Transmit(&huart2, (uint8_t *)response, response_len, 1000);
                        if (response_len < 1 || response[response_len - 1] != '\n')
                        {
                            HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, 1000);
                        }
                    }
                }
                else
                {
                    HAL_UART_Transmit(&huart2, (uint8_t *)"RX:\r\nUNKNOWN CMD\r\n", 18, 1000);
                }

                memset(cmd_buf, 0, sizeof(cmd_buf));
                cmd_idx = 0;
            }
        }
        else
        {
            if (cmd_idx < sizeof(cmd_buf) - 1)
            {
                cmd_buf[cmd_idx++] = (char)rx_ch;
            }
            else
            {
                cmd_idx = 0;
                memset(cmd_buf, 0, sizeof(cmd_buf));
                HAL_UART_Transmit(&huart2, (uint8_t *)"\r\nCMD TOO LONG\r\n", 16, 1000);
            }
        }
    }
}
