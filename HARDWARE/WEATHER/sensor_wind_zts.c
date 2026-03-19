#include "sensor_wind_zts.h"
#include "bsp_uart.h"
#include "modbus_rtu.h"
#include "weather_data.h"
#include "stm32h7xx_hal.h"

#define WIND_REG_ADDR_SPEED      0x0000U
#define WIND_REG_ADDR_DIRECTION  0x0000U
#define WIND_REG_NUM             1U
#define WIND_SLAVE_SPEED         0x01U
#define WIND_SLAVE_DIRECTION     0x02U
#define WIND_RESPONSE_LEN(reg_num) (5U + ((reg_num) * 2U))
#define WIND_RESPONSE_TIMEOUT_MS 120U

WindSensor_t g_wind_sensor = {
    .slave_addr_speed = WIND_SLAVE_SPEED,
    .slave_addr_dir   = WIND_SLAVE_DIRECTION
};

static uint8_t tx_buf[16];
static uint8_t rx_buf[32];

void WindSensor_Init(void)
{
    g_wind_sensor.base.online = 0U;
    g_wind_sensor.base.last_update_tick = 0U;
    g_wind_sensor.wind_speed_mps = 0.0f;
    g_wind_sensor.wind_dir_deg = 0.0f;
}

static uint8_t WindSensor_ReadOne(UartDev_t *uart,
                                  uint8_t slave,
                                  uint16_t reg_addr,
                                  uint16_t reg_num,
                                  uint16_t *regs)
{
    const uint16_t expected_len = WIND_RESPONSE_LEN(reg_num);
    uint16_t tx_len = Modbus_BuildReadHoldingRegs(slave, reg_addr, reg_num, tx_buf);
    uint32_t start_tick = 0U;

    UartRingBuf_Clear(&uart->rx_rb);
    BSP_Uart_Send(uart, tx_buf, tx_len);

    start_tick = HAL_GetTick();
    while ((HAL_GetTick() - start_tick) < WIND_RESPONSE_TIMEOUT_MS)
    {
        if (UartRingBuf_Available(&uart->rx_rb) >= expected_len)
        {
            break;
        }
    }

    return Modbus_ParseReadHoldingRegsResp(
        rx_buf,
        UartRingBuf_Read(&uart->rx_rb, rx_buf, sizeof(rx_buf)),
        slave,
        reg_num,
        regs);
}

void WindSensor_Poll(void)
{
    uint16_t regs[2] = {0};
    uint8_t any_success = 0U;

    if (WindSensor_ReadOne(&g_uart_wind, g_wind_sensor.slave_addr_speed,
                           WIND_REG_ADDR_SPEED, WIND_REG_NUM, regs) != 0U)
    {
        g_wind_sensor.wind_speed_mps = regs[0] / 10.0f;
        g_weather_data.wind_speed_mps = g_wind_sensor.wind_speed_mps;
        g_weather_data.wind_online = 1U;
        any_success = 1U;
    }

    if (WindSensor_ReadOne(&g_uart_wind, g_wind_sensor.slave_addr_dir,
                           WIND_REG_ADDR_DIRECTION, WIND_REG_NUM, regs) != 0U)
    {
        g_wind_sensor.wind_dir_deg = regs[0] / 10.0f;
        g_weather_data.wind_dir_deg = g_wind_sensor.wind_dir_deg;
        g_weather_data.wind_online = 1U;
        any_success = 1U;
    }

    if (any_success != 0U)
    {
        g_wind_sensor.base.last_update_tick = HAL_GetTick();
        g_wind_sensor.base.online = 1U;
    }
}

void WindSensor_Parse(void)
{
    /* Wind sensor is polled synchronously over Modbus RTU. */
}
