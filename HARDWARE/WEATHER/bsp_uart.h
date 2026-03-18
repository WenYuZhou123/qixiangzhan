#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "main.h"
#include "uart_ringbuf.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

typedef struct
{
    UART_HandleTypeDef *huart;
    UartRingBuf_t rx_rb;
    uint8_t rx_byte;
} UartDev_t;

extern UartDev_t g_uart_wind;
extern UartDev_t g_uart_air;
extern UartDev_t g_uart_rain;

void BSP_Uart_Init(void);
void BSP_Uart_StartRecvIT(UartDev_t *dev);
void BSP_Uart_Send(UartDev_t *dev, uint8_t *data, uint16_t len);

#endif
