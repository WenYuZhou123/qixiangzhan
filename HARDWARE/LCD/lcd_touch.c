#include "lcd_touch.h"

#include "lcd_port.h"
#include "main.h"

#include <stddef.h>

#define LCD_TOUCH_IIC_ADDR            0x38U
#define LCD_TOUCH_MAX_POINTS          5U

#define LCD_TOUCH_REG_DEVICE_MODE     0x00U
#define LCD_TOUCH_REG_G_MODE          0xA4U
#define LCD_TOUCH_REG_THGROUP         0x80U
#define LCD_TOUCH_REG_PERIODACTIVE    0x88U
#define LCD_TOUCH_REG_TD_STATUS       0x02U
#define LCD_TOUCH_REG_TP1             0x03U

#define LCD_TOUCH_SCL_PORT            GPIOH
#define LCD_TOUCH_SCL_PIN             GPIO_PIN_6
#define LCD_TOUCH_SDA_PORT            GPIOI
#define LCD_TOUCH_SDA_PIN             GPIO_PIN_3
#define LCD_TOUCH_INT_PORT            GPIOH
#define LCD_TOUCH_INT_PIN             GPIO_PIN_7
#define LCD_TOUCH_RST_PORT            GPIOI
#define LCD_TOUCH_RST_PIN             GPIO_PIN_8

static uint8_t s_touch_ready = 0U;

static void lcd_touch_delay_short(void)
{
    volatile uint32_t count;
    for (count = 0U; count < 80U; count++)
    {
        __NOP();
    }
}

static void lcd_touch_scl(uint8_t level)
{
    HAL_GPIO_WritePin(LCD_TOUCH_SCL_PORT, LCD_TOUCH_SCL_PIN, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void lcd_touch_sda(uint8_t level)
{
    HAL_GPIO_WritePin(LCD_TOUCH_SDA_PORT, LCD_TOUCH_SDA_PIN, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static uint8_t lcd_touch_read_sda(void)
{
    return (uint8_t)((HAL_GPIO_ReadPin(LCD_TOUCH_SDA_PORT, LCD_TOUCH_SDA_PIN) == GPIO_PIN_SET) ? 1U : 0U);
}

static void lcd_touch_iic_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();

    gpio.Pin = LCD_TOUCH_SCL_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(LCD_TOUCH_SCL_PORT, &gpio);

    gpio.Pin = LCD_TOUCH_SDA_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    HAL_GPIO_Init(LCD_TOUCH_SDA_PORT, &gpio);

    lcd_touch_sda(1U);
    lcd_touch_scl(1U);
    lcd_touch_delay_short();
}

static void lcd_touch_hw_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();

    gpio.Pin = LCD_TOUCH_INT_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LCD_TOUCH_INT_PORT, &gpio);

    gpio.Pin = LCD_TOUCH_RST_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(LCD_TOUCH_RST_PORT, &gpio);

    HAL_GPIO_WritePin(LCD_TOUCH_RST_PORT, LCD_TOUCH_RST_PIN, GPIO_PIN_SET);
}

static void lcd_touch_reset(void)
{
    HAL_GPIO_WritePin(LCD_TOUCH_RST_PORT, LCD_TOUCH_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(20U);
    HAL_GPIO_WritePin(LCD_TOUCH_RST_PORT, LCD_TOUCH_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(50U);
}

static void lcd_touch_iic_start(void)
{
    lcd_touch_sda(1U);
    lcd_touch_scl(1U);
    lcd_touch_delay_short();
    lcd_touch_sda(0U);
    lcd_touch_delay_short();
    lcd_touch_scl(0U);
    lcd_touch_delay_short();
}

static void lcd_touch_iic_stop(void)
{
    lcd_touch_sda(0U);
    lcd_touch_delay_short();
    lcd_touch_scl(1U);
    lcd_touch_delay_short();
    lcd_touch_sda(1U);
    lcd_touch_delay_short();
}

static uint8_t lcd_touch_iic_wait_ack(void)
{
    uint16_t wait = 0U;

    lcd_touch_sda(1U);
    lcd_touch_delay_short();
    lcd_touch_scl(1U);
    lcd_touch_delay_short();

    while (lcd_touch_read_sda() != 0U)
    {
        wait++;
        if (wait > 250U)
        {
            lcd_touch_iic_stop();
            return 1U;
        }
    }

    lcd_touch_scl(0U);
    lcd_touch_delay_short();
    return 0U;
}

static void lcd_touch_iic_ack(void)
{
    lcd_touch_sda(0U);
    lcd_touch_delay_short();
    lcd_touch_scl(1U);
    lcd_touch_delay_short();
    lcd_touch_scl(0U);
    lcd_touch_delay_short();
    lcd_touch_sda(1U);
}

static void lcd_touch_iic_nack(void)
{
    lcd_touch_sda(1U);
    lcd_touch_delay_short();
    lcd_touch_scl(1U);
    lcd_touch_delay_short();
    lcd_touch_scl(0U);
    lcd_touch_delay_short();
}

static void lcd_touch_iic_send_byte(uint8_t data)
{
    uint8_t i;

    for (i = 0U; i < 8U; i++)
    {
        lcd_touch_sda((uint8_t)((data & 0x80U) != 0U));
        lcd_touch_delay_short();
        lcd_touch_scl(1U);
        lcd_touch_delay_short();
        lcd_touch_scl(0U);
        data <<= 1;
    }
    lcd_touch_sda(1U);
}

static uint8_t lcd_touch_iic_recv_byte(uint8_t ack)
{
    uint8_t i;
    uint8_t data = 0U;

    for (i = 0U; i < 8U; i++)
    {
        data <<= 1;
        lcd_touch_scl(1U);
        lcd_touch_delay_short();
        if (lcd_touch_read_sda() != 0U)
        {
            data++;
        }
        lcd_touch_scl(0U);
        lcd_touch_delay_short();
    }

    if (ack != 0U)
    {
        lcd_touch_iic_ack();
    }
    else
    {
        lcd_touch_iic_nack();
    }

    return data;
}

static uint8_t lcd_touch_write_reg(uint8_t reg, uint8_t value)
{
    lcd_touch_iic_start();
    lcd_touch_iic_send_byte((uint8_t)(LCD_TOUCH_IIC_ADDR << 1));
    if (lcd_touch_iic_wait_ack() != 0U)
    {
        return 0U;
    }

    lcd_touch_iic_send_byte(reg);
    if (lcd_touch_iic_wait_ack() != 0U)
    {
        return 0U;
    }

    lcd_touch_iic_send_byte(value);
    if (lcd_touch_iic_wait_ack() != 0U)
    {
        return 0U;
    }

    lcd_touch_iic_stop();
    return 1U;
}

static void lcd_touch_read_reg(uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;

    if ((buf == NULL) || (len == 0U))
    {
        return;
    }

    lcd_touch_iic_start();
    lcd_touch_iic_send_byte((uint8_t)(LCD_TOUCH_IIC_ADDR << 1));
    (void)lcd_touch_iic_wait_ack();
    lcd_touch_iic_send_byte(reg);
    (void)lcd_touch_iic_wait_ack();

    lcd_touch_iic_start();
    lcd_touch_iic_send_byte((uint8_t)((LCD_TOUCH_IIC_ADDR << 1) | 0x01U));
    (void)lcd_touch_iic_wait_ack();

    for (i = 0U; i < (uint8_t)(len - 1U); i++)
    {
        buf[i] = lcd_touch_iic_recv_byte(1U);
    }
    buf[len - 1U] = lcd_touch_iic_recv_byte(0U);
    lcd_touch_iic_stop();
}

static void lcd_touch_reg_init(void)
{
    (void)lcd_touch_write_reg(LCD_TOUCH_REG_DEVICE_MODE, 0x00U);
    (void)lcd_touch_write_reg(LCD_TOUCH_REG_G_MODE, 0x00U);
    (void)lcd_touch_write_reg(LCD_TOUCH_REG_THGROUP, 22U);
    (void)lcd_touch_write_reg(LCD_TOUCH_REG_PERIODACTIVE, 12U);
}

void LCD_Touch_Init(void)
{
    lcd_touch_hw_init();
    lcd_touch_iic_init();
    lcd_touch_reset();
    lcd_touch_reg_init();
    s_touch_ready = 1U;
}

uint8_t LCD_Touch_Read(LCD_TouchState_t *state)
{
    uint8_t status = 0U;
    uint8_t tp[4];
    uint16_t raw_x;
    uint16_t raw_y;
    uint16_t width;
    uint16_t height;

    if (state == 0)
    {
        return 0U;
    }

    state->x = 0U;
    state->y = 0U;
    state->pressed = 0U;

    if ((s_touch_ready == 0U) || (LCD_Port_IsReady() == 0U))
    {
        return 0U;
    }

    lcd_touch_read_reg(LCD_TOUCH_REG_TD_STATUS, &status, 1U);
    status &= 0x0FU;
    if ((status == 0U) || (status > LCD_TOUCH_MAX_POINTS))
    {
        return 0U;
    }

    lcd_touch_read_reg(LCD_TOUCH_REG_TP1, tp, sizeof(tp));
    raw_x = (uint16_t)(((tp[0] & 0x0FU) << 8) | tp[1]);
    raw_y = (uint16_t)(((tp[2] & 0x0FU) << 8) | tp[3]);

    width = LCD_Port_GetWidth();
    height = LCD_Port_GetHeight();

    switch (LCD_Port_GetDisplayDir())
    {
    case LCD_PORT_DISP_DIR_0:
        state->x = (raw_x <= width) ? (uint16_t)(width - raw_x) : 0U;
        state->y = raw_y;
        break;

    case LCD_PORT_DISP_DIR_90:
        state->x = raw_y;
        state->y = raw_x;
        break;

    case LCD_PORT_DISP_DIR_180:
        state->x = raw_x;
        state->y = (raw_y <= height) ? (uint16_t)(height - raw_y) : 0U;
        break;

    case LCD_PORT_DISP_DIR_270:
    default:
        state->x = (raw_y <= width) ? (uint16_t)(width - raw_y) : 0U;
        state->y = (raw_x <= height) ? (uint16_t)(height - raw_x) : 0U;
        break;
    }

    if (state->x >= width)
    {
        state->x = (uint16_t)(width - 1U);
    }
    if (state->y >= height)
    {
        state->y = (uint16_t)(height - 1U);
    }

    state->pressed = 1U;
    return 1U;
}
