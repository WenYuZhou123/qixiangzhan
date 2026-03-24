#include "board_rs485.h"

#define BOARD_RS485_I2C_PORT          GPIOH
#define BOARD_RS485_I2C_SCL_PIN       GPIO_PIN_4
#define BOARD_RS485_I2C_SDA_PIN       GPIO_PIN_5
#define BOARD_RS485_INT_PORT          GPIOB
#define BOARD_RS485_INT_PIN           GPIO_PIN_12

#define BOARD_RS485_PCF8574_ADDR      0x40U
#define BOARD_RS485_RE_BIT            6U

#define BOARD_RS485_I2C_DELAY_US      4U
#define BOARD_RS485_ACK_TIMEOUT_US    250U

static uint8_t s_board_rs485_ready = 0U;
static uint8_t s_board_rs485_state = 0xFFU;
static uint8_t s_board_rs485_dwt_ready = 0U;

static void BoardRS485_EnableCycleCounter(void)
{
    if (s_board_rs485_dwt_ready != 0U)
    {
        return;
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    s_board_rs485_dwt_ready = 1U;
}

static void BoardRS485_DelayUs(uint32_t delay_us)
{
    uint32_t delay_cycles;
    uint32_t start_cycle;

    if (delay_us == 0U)
    {
        return;
    }

    BoardRS485_EnableCycleCounter();
    delay_cycles = delay_us * (SystemCoreClock / 1000000U);
    if (delay_cycles == 0U)
    {
        delay_cycles = 1U;
    }

    start_cycle = DWT->CYCCNT;
    while ((DWT->CYCCNT - start_cycle) < delay_cycles)
    {
    }
}

static void BoardRS485_SCL(uint8_t high)
{
    HAL_GPIO_WritePin(BOARD_RS485_I2C_PORT,
                      BOARD_RS485_I2C_SCL_PIN,
                      (high != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void BoardRS485_SDA(uint8_t high)
{
    HAL_GPIO_WritePin(BOARD_RS485_I2C_PORT,
                      BOARD_RS485_I2C_SDA_PIN,
                      (high != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static GPIO_PinState BoardRS485_ReadSDA(void)
{
    return HAL_GPIO_ReadPin(BOARD_RS485_I2C_PORT, BOARD_RS485_I2C_SDA_PIN);
}

static void BoardRS485_SDA_Output(void)
{
    BOARD_RS485_I2C_PORT->MODER &= ~(3UL << (5U * 2U));
    BOARD_RS485_I2C_PORT->MODER |= (1UL << (5U * 2U));
}

static void BoardRS485_SDA_Input(void)
{
    BOARD_RS485_I2C_PORT->MODER &= ~(3UL << (5U * 2U));
}

static void BoardRS485_I2C_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOH_CLK_ENABLE();

    gpio.Pin = BOARD_RS485_I2C_SCL_PIN | BOARD_RS485_I2C_SDA_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(BOARD_RS485_I2C_PORT, &gpio);

    BoardRS485_SDA(1U);
    BoardRS485_SCL(1U);
}

static void BoardRS485_I2C_Start(void)
{
    BoardRS485_SDA_Output();
    BoardRS485_SDA(1U);
    BoardRS485_SCL(1U);
    BoardRS485_DelayUs(BOARD_RS485_I2C_DELAY_US);
    BoardRS485_SDA(0U);
    BoardRS485_DelayUs(BOARD_RS485_I2C_DELAY_US);
    BoardRS485_SCL(0U);
}

static void BoardRS485_I2C_Stop(void)
{
    BoardRS485_SDA_Output();
    BoardRS485_SCL(0U);
    BoardRS485_SDA(0U);
    BoardRS485_DelayUs(BOARD_RS485_I2C_DELAY_US);
    BoardRS485_SCL(1U);
    BoardRS485_SDA(1U);
    BoardRS485_DelayUs(BOARD_RS485_I2C_DELAY_US);
}

static HAL_StatusTypeDef BoardRS485_I2C_WaitAck(void)
{
    uint32_t timeout = 0U;

    BoardRS485_SDA_Input();
    BoardRS485_SDA(1U);
    BoardRS485_DelayUs(1U);
    BoardRS485_SCL(1U);
    BoardRS485_DelayUs(1U);

    while (BoardRS485_ReadSDA() == GPIO_PIN_SET)
    {
        timeout++;
        if (timeout > BOARD_RS485_ACK_TIMEOUT_US)
        {
            BoardRS485_I2C_Stop();
            return HAL_ERROR;
        }
    }

    BoardRS485_SCL(0U);
    return HAL_OK;
}

static void BoardRS485_I2C_SendByte(uint8_t value)
{
    uint8_t bit_index;

    BoardRS485_SDA_Output();
    BoardRS485_SCL(0U);
    for (bit_index = 0U; bit_index < 8U; bit_index++)
    {
        BoardRS485_SDA((value & 0x80U) != 0U);
        value <<= 1;
        BoardRS485_DelayUs(2U);
        BoardRS485_SCL(1U);
        BoardRS485_DelayUs(2U);
        BoardRS485_SCL(0U);
        BoardRS485_DelayUs(2U);
    }
}

static HAL_StatusTypeDef BoardRS485_PCF8574_WriteState(uint8_t state)
{
    BoardRS485_I2C_Start();
    BoardRS485_I2C_SendByte(BOARD_RS485_PCF8574_ADDR);
    if (BoardRS485_I2C_WaitAck() != HAL_OK)
    {
        return HAL_ERROR;
    }

    BoardRS485_I2C_SendByte(state);
    if (BoardRS485_I2C_WaitAck() != HAL_OK)
    {
        return HAL_ERROR;
    }

    BoardRS485_I2C_Stop();
    s_board_rs485_state = state;
    return HAL_OK;
}

static HAL_StatusTypeDef BoardRS485_SetTxMode(uint8_t enable)
{
    uint8_t next_state;

    if (s_board_rs485_ready == 0U)
    {
        return HAL_ERROR;
    }

    next_state = s_board_rs485_state;
    if (enable != 0U)
    {
        next_state |= (uint8_t)(1U << BOARD_RS485_RE_BIT);
    }
    else
    {
        next_state &= (uint8_t)~(1U << BOARD_RS485_RE_BIT);
    }

    return BoardRS485_PCF8574_WriteState(next_state);
}

HAL_StatusTypeDef BoardRS485_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin = BOARD_RS485_INT_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(BOARD_RS485_INT_PORT, &gpio);

    BoardRS485_I2C_Init();
    s_board_rs485_ready = 0U;
    s_board_rs485_state = 0xFFU;

    BoardRS485_I2C_Start();
    BoardRS485_I2C_SendByte(BOARD_RS485_PCF8574_ADDR);
    if (BoardRS485_I2C_WaitAck() != HAL_OK)
    {
        return HAL_ERROR;
    }
    BoardRS485_I2C_Stop();

    if (BoardRS485_PCF8574_WriteState(0xFFU) != HAL_OK)
    {
        return HAL_ERROR;
    }

    s_board_rs485_ready = 1U;
    if (BoardRS485_SetTxMode(0U) != HAL_OK)
    {
        s_board_rs485_ready = 0U;
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef BoardRS485_Transmit(UART_HandleTypeDef *huart,
                                      const uint8_t *data,
                                      uint16_t len,
                                      uint32_t timeout_ms)
{
    uint32_t start_tick;

    if (huart == NULL || data == NULL || len == 0U)
    {
        return HAL_ERROR;
    }
    if (s_board_rs485_ready == 0U)
    {
        return HAL_ERROR;
    }

    __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_TCF);
    if (BoardRS485_SetTxMode(1U) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (HAL_UART_Transmit(huart, (uint8_t *)data, len, timeout_ms) != HAL_OK)
    {
        (void)BoardRS485_SetTxMode(0U);
        return HAL_ERROR;
    }

    start_tick = HAL_GetTick();
    while (__HAL_UART_GET_FLAG(huart, UART_FLAG_TC) == RESET)
    {
        if ((HAL_GetTick() - start_tick) >= timeout_ms)
        {
            (void)BoardRS485_SetTxMode(0U);
            return HAL_TIMEOUT;
        }
    }

    HAL_Delay(1U);
    if (BoardRS485_SetTxMode(0U) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

uint8_t BoardRS485_IsReady(void)
{
    return s_board_rs485_ready;
}
