#include "sensor_wind_zts.h"
#include "bsp_uart.h"
#include "modbus_rtu.h"
#include "weather_data.h"
#include "stm32h7xx_hal.h"

#define WIND_REG_ADDR_VALUE   0x0000   // 占位，后面按你手册改
#define WIND_REG_NUM          1

WindSensor_t g_wind_sensor = {
    .slave_addr_speed = 0x01,
    .slave_addr_dir   = 0x02
};

static uint8_t tx_buf[16];
static uint8_t rx_buf[32];

void WindSensor_Init(void)
{
    g_wind_sensor.base.online = 0;
    g_wind_sensor.wind_speed_mps = 0.0f;
    g_wind_sensor.wind_dir_deg = 0.0f;
}

static uint8_t WindSensor_ReadOne(UartDev_t *uart,
                                  uint8_t slave,
                                  uint16_t reg_addr,
                                  uint16_t reg_num,
                                  uint16_t *regs)
{
    uint16_t tx_len = Modbus_BuildReadHoldingRegs(slave, reg_addr, reg_num, tx_buf);

    UartRingBuf_Clear(&uart->rx_rb);
    BSP_Uart_Send(uart, tx_buf, tx_len);
    HAL_Delay(20);   // H743 裸机先这么写，后面可换状态机

    uint16_t rx_len = UartRingBuf_Read(&uart->rx_rb, rx_buf, sizeof(rx_buf));
    return Modbus_ParseReadHoldingRegsResp(rx_buf, rx_len, slave, reg_num, regs);
}

void WindSensor_Poll(void)
{
    uint16_t regs[2] = {0};

    if(WindSensor_ReadOne(&g_uart_wind, g_wind_sensor.slave_addr_speed,
                          WIND_REG_ADDR_VALUE, WIND_REG_NUM, regs))
    {
        // 假设原始值 = 0.1m/s
        g_wind_sensor.wind_speed_mps = regs[0] / 10.0f;
        g_weather_data.wind_speed_mps = g_wind_sensor.wind_speed_mps;
        g_weather_data.wind_online = 1;
    }

    if(WindSensor_ReadOne(&g_uart_wind, g_wind_sensor.slave_addr_dir,
                          WIND_REG_ADDR_VALUE, WIND_REG_NUM, regs))
    {
        // 假设原始值 = 0.1°
        g_wind_sensor.wind_dir_deg = regs[0] / 10.0f;
        g_weather_data.wind_dir_deg = g_wind_sensor.wind_dir_deg;
        g_weather_data.wind_online = 1;
    }

    g_wind_sensor.base.last_update_tick = HAL_GetTick();
}

void WindSensor_Parse(void)
{
    // 当前轮询同步读，不单独解析
}
