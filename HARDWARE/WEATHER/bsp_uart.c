#include "bsp_uart.h"

UartDev_t g_uart_air  = {0};

void BSP_Uart_Init(void)
{
    g_uart_air.huart  = &huart3;

    UartRingBuf_Init(&g_uart_air.rx_rb);

    BSP_Uart_StartRecvIT(&g_uart_air);
}

void BSP_Uart_StartRecvIT(UartDev_t *dev)
{
    HAL_UART_Receive_IT(dev->huart, &dev->rx_byte, 1);
}

void BSP_Uart_Send(UartDev_t *dev, uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(dev->huart, data, len, 100);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == g_uart_air.huart)
    {
        UartRingBuf_Push(&g_uart_air.rx_rb, g_uart_air.rx_byte);
        HAL_UART_Receive_IT(g_uart_air.huart, &g_uart_air.rx_byte, 1);
    }
}
