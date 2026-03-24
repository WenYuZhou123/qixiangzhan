#include "bsp_uart.h"
#include "board_rs485.h"
#include "console.h"

UartDev_t g_uart_air  = {0};
UartDev_t g_uart_wind = {0};
static uint32_t s_uart_air_rx_count = 0U;
static uint32_t s_uart_air_rx_error_count = 0U;
static uint32_t s_uart_air_last_rx_tick = 0U;
static uint32_t s_uart_wind_rx_count = 0U;
static uint32_t s_uart_wind_rx_error_count = 0U;
static uint32_t s_uart_wind_last_rx_tick = 0U;

void BSP_Uart_Init(void)
{
    (void)BoardRS485_Init();

    g_uart_air.huart  = &huart3;
    g_uart_wind.huart = &huart2;
    s_uart_air_rx_count = 0U;
    s_uart_air_rx_error_count = 0U;
    s_uart_air_last_rx_tick = 0U;
    s_uart_wind_rx_count = 0U;
    s_uart_wind_rx_error_count = 0U;
    s_uart_wind_last_rx_tick = 0U;

    UartRingBuf_Init(&g_uart_air.rx_rb);
    UartRingBuf_Init(&g_uart_wind.rx_rb);

    BSP_Uart_StartRecvIT(&g_uart_air);
    BSP_Uart_StartRecvIT(&g_uart_wind);
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
        s_uart_air_rx_count++;
        s_uart_air_last_rx_tick = HAL_GetTick();
        HAL_UART_Receive_IT(g_uart_air.huart, &g_uart_air.rx_byte, 1);
    }
    else if (huart == g_uart_wind.huart)
    {
        UartRingBuf_Push(&g_uart_wind.rx_rb, g_uart_wind.rx_byte);
        s_uart_wind_rx_count++;
        s_uart_wind_last_rx_tick = HAL_GetTick();
        HAL_UART_Receive_IT(g_uart_wind.huart, &g_uart_wind.rx_byte, 1);
    }

    Console_OnUartRxCplt(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == g_uart_air.huart)
    {
        s_uart_air_rx_error_count++;
        __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_PEF | UART_CLEAR_FEF);
        (void)HAL_UART_AbortReceive(huart);
        UartRingBuf_Clear(&g_uart_air.rx_rb);
        (void)HAL_UART_Receive_IT(g_uart_air.huart, &g_uart_air.rx_byte, 1);
    }
    else if (huart == g_uart_wind.huart)
    {
        s_uart_wind_rx_error_count++;
        __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_PEF | UART_CLEAR_FEF);
        (void)HAL_UART_AbortReceive(huart);
        UartRingBuf_Clear(&g_uart_wind.rx_rb);
        (void)HAL_UART_Receive_IT(g_uart_wind.huart, &g_uart_wind.rx_byte, 1);
    }

    Console_OnUartError(huart);
}

uint32_t BSP_Uart_GetRxCount(const UartDev_t *dev)
{
    if (dev == &g_uart_air)
    {
        return s_uart_air_rx_count;
    }
    if (dev == &g_uart_wind)
    {
        return s_uart_wind_rx_count;
    }

    return 0U;
}

uint32_t BSP_Uart_GetRxErrorCount(const UartDev_t *dev)
{
    if (dev == &g_uart_air)
    {
        return s_uart_air_rx_error_count;
    }
    if (dev == &g_uart_wind)
    {
        return s_uart_wind_rx_error_count;
    }

    return 0U;
}

uint32_t BSP_Uart_GetLastRxTick(const UartDev_t *dev)
{
    if (dev == &g_uart_air)
    {
        return s_uart_air_last_rx_tick;
    }
    if (dev == &g_uart_wind)
    {
        return s_uart_wind_last_rx_tick;
    }

    return 0U;
}
