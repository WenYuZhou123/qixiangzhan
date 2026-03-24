#include "lcd_port.h"

#include "atk_md0700_font.h"
#include "main.h"
#include "usart.h"
#include "stm32h7xx_hal_sram.h"
#include "stm32h7xx_ll_fmc.h"

#include <stddef.h>
#include <string.h>

#define LCD_NATIVE_WIDTH              480U
#define LCD_NATIVE_HEIGHT             800U
#define LCD_DRIVER_PID                0x61U

#define LCD_FMC_BANK                  FMC_NORSRAM_BANK1
#define LCD_FMC_BANK_ADDR             0x60000000U
#define LCD_FMC_REG_SEL               18U
#define LCD_FMC_CMD_ADDR              (LCD_FMC_BANK_ADDR | (((1UL << LCD_FMC_REG_SEL) - 1UL) << 1))
#define LCD_FMC_DAT_ADDR              (LCD_FMC_BANK_ADDR | ((1UL << LCD_FMC_REG_SEL) << 1))

#define LCD_SCAN_DIR_L2R_U2D          0x0000U
#define LCD_SCAN_DIR_L2R_D2U          0x0080U
#define LCD_SCAN_DIR_R2L_U2D          0x0040U
#define LCD_SCAN_DIR_R2L_D2U          0x00C0U
#define LCD_SCAN_DIR_U2D_L2R          0x0020U
#define LCD_SCAN_DIR_U2D_R2L          0x0060U
#define LCD_SCAN_DIR_D2U_L2R          0x00A0U
#define LCD_SCAN_DIR_D2U_R2L          0x00E0U

#define LCD_FMC_CMD_REG               (*(volatile uint16_t *)LCD_FMC_CMD_ADDR)
#define LCD_FMC_DAT_REG               (*(volatile uint16_t *)LCD_FMC_DAT_ADDR)

typedef struct
{
    uint8_t ready;
    uint8_t pid;
    uint16_t width;
    uint16_t height;
    uint8_t disp_dir;
    uint16_t scan_dir;
} LCD_PortState_t;

typedef enum
{
    LCD_FONT_12 = 0U,
    LCD_FONT_16,
    LCD_FONT_24,
    LCD_FONT_32
} LCD_Font_t;

static LCD_PortState_t s_lcd = {0};
static SRAM_HandleTypeDef s_sram = {0};
static uint8_t s_delay_ready = 0U;

static void lcd_debug_print(const char *text)
{
    (void)text;
}

static void lcd_delay_init(void)
{
    if (s_delay_ready != 0U)
    {
        return;
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    s_delay_ready = 1U;
}

static void lcd_delay_us(uint32_t us)
{
    uint32_t start;
    uint32_t ticks;

    lcd_delay_init();
    start = DWT->CYCCNT;
    ticks = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks)
    {
    }
}

static void lcd_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

static inline void lcd_fmc_write_cmd(uint16_t cmd)
{
    LCD_FMC_CMD_REG = cmd;
}

static inline void lcd_fmc_write_dat(uint16_t dat)
{
    LCD_FMC_DAT_REG = dat;
}

static inline void lcd_fmc_write_reg(uint16_t reg, uint16_t dat)
{
    LCD_FMC_CMD_REG = reg;
    LCD_FMC_DAT_REG = dat;
}

static inline uint16_t lcd_fmc_read_dat(void)
{
    __NOP();
    __NOP();
    return LCD_FMC_DAT_REG;
}

static void lcd_mpu_config(void)
{
    MPU_Region_InitTypeDef region = {0};

    HAL_MPU_Disable();

    region.Enable = MPU_REGION_ENABLE;
    region.Number = MPU_REGION_NUMBER1;
    region.BaseAddress = LCD_FMC_BANK_ADDR;
    region.Size = MPU_REGION_SIZE_256MB;
    region.SubRegionDisable = 0x00U;
    region.TypeExtField = MPU_TEX_LEVEL0;
    region.AccessPermission = MPU_REGION_FULL_ACCESS;
    region.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
    region.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    region.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    region.IsBufferable = MPU_ACCESS_BUFFERABLE;
    HAL_MPU_ConfigRegion(&region);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

static void lcd_fmc_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_FMC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF12_FMC;

    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 |
               GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
               GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &gpio);

    gpio.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
               GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |
               GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &gpio);
}

static void lcd_fmc_init(void)
{
    FMC_NORSRAM_TimingTypeDef read_timing = {0};
    FMC_NORSRAM_TimingTypeDef write_timing = {0};

    lcd_fmc_gpio_init();
    lcd_mpu_config();

    s_sram.Instance = FMC_NORSRAM_DEVICE;
    s_sram.Extended = FMC_NORSRAM_EXTENDED_DEVICE;
    s_sram.Init.NSBank = LCD_FMC_BANK;
    s_sram.Init.DataAddressMux = FMC_DATA_ADDRESS_MUX_DISABLE;
    s_sram.Init.MemoryType = FMC_MEMORY_TYPE_SRAM;
    s_sram.Init.MemoryDataWidth = FMC_NORSRAM_MEM_BUS_WIDTH_16;
    s_sram.Init.BurstAccessMode = FMC_BURST_ACCESS_MODE_DISABLE;
    s_sram.Init.WaitSignalPolarity = FMC_WAIT_SIGNAL_POLARITY_LOW;
    s_sram.Init.WaitSignalActive = FMC_WAIT_TIMING_BEFORE_WS;
    s_sram.Init.WriteOperation = FMC_WRITE_OPERATION_ENABLE;
    s_sram.Init.WaitSignal = FMC_WAIT_SIGNAL_DISABLE;
    s_sram.Init.ExtendedMode = FMC_EXTENDED_MODE_ENABLE;
    s_sram.Init.AsynchronousWait = FMC_ASYNCHRONOUS_WAIT_DISABLE;
    s_sram.Init.WriteBurst = FMC_WRITE_BURST_DISABLE;
    s_sram.Init.ContinuousClock = FMC_CONTINUOUS_CLOCK_SYNC_ONLY;
    s_sram.Init.PageSize = FMC_PAGE_SIZE_NONE;

    read_timing.AddressSetupTime = 0x0FU;
    read_timing.DataSetupTime = 0x50U;
    read_timing.BusTurnAroundDuration = 0U;
    read_timing.AccessMode = FMC_ACCESS_MODE_A;

    write_timing.AddressSetupTime = 0x05U;
    write_timing.DataSetupTime = 0x05U;
    write_timing.BusTurnAroundDuration = 0U;
    write_timing.AccessMode = FMC_ACCESS_MODE_A;

    HAL_SRAM_Init(&s_sram, &read_timing, &write_timing);
}

static uint8_t lcd_get_pid(void)
{
    uint8_t pid;

    lcd_fmc_write_cmd(0xA1U);
    (void)lcd_fmc_read_dat();
    (void)lcd_fmc_read_dat();
    pid = (uint8_t)lcd_fmc_read_dat();
    return pid;
}

static void lcd_reg_init(void)
{
    lcd_fmc_write_cmd(0xE2U);
    lcd_fmc_write_dat(0x1DU);
    lcd_fmc_write_dat(0x02U);
    lcd_fmc_write_dat(0x04U);
    lcd_delay_us(100U);

    lcd_fmc_write_cmd(0xE0U);
    lcd_fmc_write_dat(0x01U);
    lcd_delay_ms(10U);

    lcd_fmc_write_cmd(0xE0U);
    lcd_fmc_write_dat(0x03U);
    lcd_delay_ms(12U);

    lcd_fmc_write_cmd(0x01U);
    lcd_delay_ms(10U);

    lcd_fmc_write_cmd(0xE6U);
    lcd_fmc_write_dat(0x2FU);
    lcd_fmc_write_dat(0xFFU);
    lcd_fmc_write_dat(0xFFU);

    lcd_fmc_write_cmd(0xB0U);
    lcd_fmc_write_dat(0x20U);
    lcd_fmc_write_dat(0x00U);
    lcd_fmc_write_dat(0x03U);
    lcd_fmc_write_dat(0x1FU);
    lcd_fmc_write_dat(0x01U);
    lcd_fmc_write_dat(0xDFU);
    lcd_fmc_write_dat(0x00U);

    lcd_fmc_write_cmd(0xB4U);
    lcd_fmc_write_dat(0x04U);
    lcd_fmc_write_dat(0x1FU);
    lcd_fmc_write_dat(0x00U);
    lcd_fmc_write_dat(0x2EU);
    lcd_fmc_write_dat(0x00U);
    lcd_fmc_write_dat(0x00U);
    lcd_fmc_write_dat(0x00U);
    lcd_fmc_write_dat(0x00U);

    lcd_fmc_write_cmd(0xB6U);
    lcd_fmc_write_dat(0x02U);
    lcd_fmc_write_dat(0x0CU);
    lcd_fmc_write_dat(0x00U);
    lcd_fmc_write_dat(0x17U);
    lcd_fmc_write_dat(0x16U);
    lcd_fmc_write_dat(0x00U);
    lcd_fmc_write_dat(0x00U);

    lcd_fmc_write_cmd(0xF0U);
    lcd_fmc_write_dat(0x03U);
    lcd_fmc_write_cmd(0x29U);
    lcd_fmc_write_cmd(0xD0U);
    lcd_fmc_write_dat(0x00U);

    lcd_fmc_write_cmd(0xBEU);
    lcd_fmc_write_dat(0x05U);
    lcd_fmc_write_dat(0xFEU);
    lcd_fmc_write_dat(0x01U);
    lcd_fmc_write_dat(0x00U);
    lcd_fmc_write_dat(0x00U);
    lcd_fmc_write_dat(0x00U);

    lcd_fmc_write_cmd(0xB8U);
    lcd_fmc_write_dat(0x03U);
    lcd_fmc_write_dat(0x01U);

    lcd_fmc_write_cmd(0xBAU);
    lcd_fmc_write_dat(0x01U);
}

static void lcd_set_column_address(uint16_t sc, uint16_t ec)
{
    if ((s_lcd.disp_dir == LCD_PORT_DISP_DIR_0) || (s_lcd.disp_dir == LCD_PORT_DISP_DIR_180))
    {
        lcd_fmc_write_cmd(0x2BU);
    }
    else
    {
        lcd_fmc_write_cmd(0x2AU);
    }

    lcd_fmc_write_dat((uint16_t)((sc >> 8) & 0xFFU));
    lcd_fmc_write_dat((uint16_t)(sc & 0xFFU));
    lcd_fmc_write_dat((uint16_t)((ec >> 8) & 0xFFU));
    lcd_fmc_write_dat((uint16_t)(ec & 0xFFU));
}

static void lcd_set_page_address(uint16_t sp, uint16_t ep)
{
    if ((s_lcd.disp_dir == LCD_PORT_DISP_DIR_0) || (s_lcd.disp_dir == LCD_PORT_DISP_DIR_180))
    {
        lcd_fmc_write_cmd(0x2AU);
    }
    else
    {
        lcd_fmc_write_cmd(0x2BU);
    }

    lcd_fmc_write_dat((uint16_t)((sp >> 8) & 0xFFU));
    lcd_fmc_write_dat((uint16_t)(sp & 0xFFU));
    lcd_fmc_write_dat((uint16_t)((ep >> 8) & 0xFFU));
    lcd_fmc_write_dat((uint16_t)(ep & 0xFFU));
}

static void lcd_start_write_memory(void)
{
    lcd_fmc_write_cmd(0x2CU);
}

static uint8_t lcd_set_scan_dir(uint16_t scan_dir)
{
    uint16_t reg36;

    switch (s_lcd.disp_dir)
    {
    case LCD_PORT_DISP_DIR_0:
        switch (scan_dir)
        {
        case LCD_SCAN_DIR_L2R_U2D: reg36 = LCD_SCAN_DIR_U2D_L2R; break;
        case LCD_SCAN_DIR_L2R_D2U: reg36 = LCD_SCAN_DIR_U2D_R2L; break;
        case LCD_SCAN_DIR_R2L_U2D: reg36 = LCD_SCAN_DIR_D2U_L2R; break;
        case LCD_SCAN_DIR_R2L_D2U: reg36 = LCD_SCAN_DIR_D2U_R2L; break;
        case LCD_SCAN_DIR_U2D_L2R: reg36 = LCD_SCAN_DIR_L2R_U2D; break;
        case LCD_SCAN_DIR_U2D_R2L: reg36 = LCD_SCAN_DIR_L2R_D2U; break;
        case LCD_SCAN_DIR_D2U_L2R: reg36 = LCD_SCAN_DIR_R2L_U2D; break;
        case LCD_SCAN_DIR_D2U_R2L: reg36 = LCD_SCAN_DIR_R2L_D2U; break;
        default: return 0U;
        }
        reg36 |= 0x01U;
        break;

    case LCD_PORT_DISP_DIR_90:
        reg36 = scan_dir;
        break;

    case LCD_PORT_DISP_DIR_180:
        switch (scan_dir)
        {
        case LCD_SCAN_DIR_L2R_U2D: reg36 = LCD_SCAN_DIR_U2D_L2R; break;
        case LCD_SCAN_DIR_L2R_D2U: reg36 = LCD_SCAN_DIR_U2D_R2L; break;
        case LCD_SCAN_DIR_R2L_U2D: reg36 = LCD_SCAN_DIR_D2U_L2R; break;
        case LCD_SCAN_DIR_R2L_D2U: reg36 = LCD_SCAN_DIR_D2U_R2L; break;
        case LCD_SCAN_DIR_U2D_L2R: reg36 = LCD_SCAN_DIR_L2R_U2D; break;
        case LCD_SCAN_DIR_U2D_R2L: reg36 = LCD_SCAN_DIR_L2R_D2U; break;
        case LCD_SCAN_DIR_D2U_L2R: reg36 = LCD_SCAN_DIR_R2L_U2D; break;
        case LCD_SCAN_DIR_D2U_R2L: reg36 = LCD_SCAN_DIR_R2L_D2U; break;
        default: return 0U;
        }
        reg36 |= 0x02U;
        break;

    case LCD_PORT_DISP_DIR_270:
        reg36 = (uint16_t)(scan_dir | 0x03U);
        break;

    default:
        return 0U;
    }

    s_lcd.scan_dir = reg36;
    lcd_fmc_write_reg(0x36U, reg36);
    lcd_set_column_address(0U, (uint16_t)(s_lcd.width - 1U));
    lcd_set_page_address(0U, (uint16_t)(s_lcd.height - 1U));
    return 1U;
}

static uint8_t lcd_set_disp_dir(uint8_t disp_dir)
{
    switch (disp_dir)
    {
    case LCD_PORT_DISP_DIR_0:
    case LCD_PORT_DISP_DIR_180:
        s_lcd.width = LCD_NATIVE_WIDTH;
        s_lcd.height = LCD_NATIVE_HEIGHT;
        break;

    case LCD_PORT_DISP_DIR_90:
    case LCD_PORT_DISP_DIR_270:
        s_lcd.width = LCD_NATIVE_HEIGHT;
        s_lcd.height = LCD_NATIVE_WIDTH;
        break;

    default:
        return 0U;
    }

    s_lcd.disp_dir = disp_dir;
    return lcd_set_scan_dir(LCD_SCAN_DIR_L2R_U2D);
}

static void lcd_fill_region(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color)
{
    uint16_t x;
    uint16_t y;

    if (s_lcd.ready == 0U)
    {
        return;
    }

    if ((xs >= s_lcd.width) || (ys >= s_lcd.height))
    {
        return;
    }

    if (xe >= s_lcd.width)
    {
        xe = (uint16_t)(s_lcd.width - 1U);
    }
    if (ye >= s_lcd.height)
    {
        ye = (uint16_t)(s_lcd.height - 1U);
    }
    if ((xe < xs) || (ye < ys))
    {
        return;
    }

    lcd_set_column_address(xs, xe);
    lcd_set_page_address(ys, ye);
    lcd_start_write_memory();

    for (y = ys; y <= ye; y++)
    {
        for (x = xs; x <= xe; x++)
        {
            lcd_fmc_write_dat(color);
        }
    }
}

static void lcd_draw_point(uint16_t x, uint16_t y, uint16_t color)
{
    if ((s_lcd.ready == 0U) || (x >= s_lcd.width) || (y >= s_lcd.height))
    {
        return;
    }

    lcd_set_column_address(x, x);
    lcd_set_page_address(y, y);
    lcd_start_write_memory();
    lcd_fmc_write_dat(color);
}

static void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    int32_t dx = (x2 >= x1) ? (int32_t)(x2 - x1) : (int32_t)(x1 - x2);
    int32_t sx = (x1 < x2) ? 1 : -1;
    int32_t dy = (y2 >= y1) ? -(int32_t)(y2 - y1) : -(int32_t)(y1 - y2);
    int32_t sy = (y1 < y2) ? 1 : -1;
    int32_t err = dx + dy;

    while (1)
    {
        lcd_draw_point(x1, y1, color);
        if ((x1 == x2) && (y1 == y2))
        {
            break;
        }

        if ((err << 1) >= dy)
        {
            err += dy;
            x1 = (uint16_t)((int32_t)x1 + sx);
        }
        if ((err << 1) <= dx)
        {
            err += dx;
            y1 = (uint16_t)((int32_t)y1 + sy);
        }
    }
}

static void lcd_get_font_desc(LCD_Font_t font,
                              const uint8_t **glyph,
                              uint8_t *char_width,
                              uint8_t *char_height,
                              uint8_t *char_size,
                              char ch)
{
    uint8_t offset = (uint8_t)(ch - ' ');

    *glyph = NULL;
    *char_width = 0U;
    *char_height = 0U;
    *char_size = 0U;

    if ((ch < ' ') || (ch > '~'))
    {
        return;
    }

    switch (font)
    {
    case LCD_FONT_12:
        *glyph = atk_md0700_font_1206[offset];
        *char_width = ATK_MD0700_FONT_12_CHAR_WIDTH;
        *char_height = ATK_MD0700_FONT_12_CHAR_HEIGHT;
        *char_size = ATK_MD0700_FONT_12_CHAR_SIZE;
        break;

    case LCD_FONT_16:
        *glyph = atk_md0700_font_1608[offset];
        *char_width = ATK_MD0700_FONT_16_CHAR_WIDTH;
        *char_height = ATK_MD0700_FONT_16_CHAR_HEIGHT;
        *char_size = ATK_MD0700_FONT_16_CHAR_SIZE;
        break;

    case LCD_FONT_24:
        *glyph = atk_md0700_font_2412[offset];
        *char_width = ATK_MD0700_FONT_24_CHAR_WIDTH;
        *char_height = ATK_MD0700_FONT_24_CHAR_HEIGHT;
        *char_size = ATK_MD0700_FONT_24_CHAR_SIZE;
        break;

    case LCD_FONT_32:
    default:
        *glyph = atk_md0700_font_3216[offset];
        *char_width = ATK_MD0700_FONT_32_CHAR_WIDTH;
        *char_height = ATK_MD0700_FONT_32_CHAR_HEIGHT;
        *char_size = ATK_MD0700_FONT_32_CHAR_SIZE;
        break;
    }
}

static void lcd_show_char(uint16_t x, uint16_t y, char ch, LCD_Font_t font, uint16_t color)
{
    const uint8_t *glyph;
    uint8_t char_width;
    uint8_t char_height;
    uint8_t char_size;
    uint8_t byte_index;
    uint8_t bit_index;
    uint8_t width_index = 0U;
    uint8_t height_index = 0U;
    uint8_t byte_code;

    lcd_get_font_desc(font, &glyph, &char_width, &char_height, &char_size, ch);
    if ((glyph == NULL) ||
        ((uint32_t)x + char_width > s_lcd.width) ||
        ((uint32_t)y + char_height > s_lcd.height))
    {
        return;
    }

    for (byte_index = 0U; byte_index < char_size; byte_index++)
    {
        byte_code = glyph[byte_index];
        for (bit_index = 0U; bit_index < 8U; bit_index++)
        {
            if ((byte_code & 0x80U) != 0U)
            {
                lcd_draw_point((uint16_t)(x + width_index), (uint16_t)(y + height_index), color);
            }

            height_index++;
            if (height_index == char_height)
            {
                height_index = 0U;
                width_index++;
                break;
            }
            byte_code <<= 1;
        }
    }
}

static void lcd_show_string(uint16_t x,
                            uint16_t y,
                            uint16_t width,
                            uint16_t height,
                            const char *str,
                            LCD_Font_t font,
                            uint16_t color)
{
    uint8_t char_width = 0U;
    uint8_t char_height = 0U;
    uint16_t x_raw = x;
    uint16_t x_limit;
    uint16_t y_limit;
    const uint8_t *dummy;
    uint8_t dummy_size;

    if ((str == NULL) || (s_lcd.ready == 0U))
    {
        return;
    }

    lcd_get_font_desc(font, &dummy, &char_width, &char_height, &dummy_size, 'A');
    if ((char_width == 0U) || (char_height == 0U))
    {
        return;
    }

    x_limit = (((uint32_t)x + width) > s_lcd.width) ? s_lcd.width : (uint16_t)(x + width);
    y_limit = (((uint32_t)y + height) > s_lcd.height) ? s_lcd.height : (uint16_t)(y + height);

    while ((*str >= ' ') && (*str <= '~'))
    {
        if ((uint32_t)x + char_width > x_limit)
        {
            x = x_raw;
            y = (uint16_t)(y + char_height);
        }
        if ((uint32_t)y + char_height > y_limit)
        {
            break;
        }

        lcd_show_char(x, y, *str, font, color);
        x = (uint16_t)(x + char_width);
        str++;
    }
}

void LCD_Port_Init(void)
{
    uint8_t pid;

    lcd_delay_init();
    lcd_fmc_init();

    pid = lcd_get_pid();
    if (pid != LCD_DRIVER_PID)
    {
        s_lcd.ready = 0U;
        lcd_debug_print("LCD PID FAIL\r\n");
        return;
    }

    s_lcd.pid = pid;
    lcd_reg_init();
    (void)lcd_set_disp_dir(LCD_PORT_DISP_DIR_270);
    LCD_Port_SetBacklight(100U);
    s_lcd.ready = 1U;
    lcd_fill_region(0U, 0U, (uint16_t)(s_lcd.width - 1U), (uint16_t)(s_lcd.height - 1U), 0xFFFFU);
}

void LCD_Port_BeginFrame(void)
{
}

void LCD_Port_EndFrame(void)
{
}

void LCD_Port_Clear(uint16_t color)
{
    if (s_lcd.ready == 0U)
    {
        return;
    }

    lcd_fill_region(0U, 0U, (uint16_t)(s_lcd.width - 1U), (uint16_t)(s_lcd.height - 1U), color);
}

void LCD_Port_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if ((w == 0U) || (h == 0U))
    {
        return;
    }

    lcd_fill_region(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U), color);
}

void LCD_Port_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint16_t x2;
    uint16_t y2;

    if ((w == 0U) || (h == 0U) || (s_lcd.ready == 0U))
    {
        return;
    }

    x2 = (uint16_t)(x + w - 1U);
    y2 = (uint16_t)(y + h - 1U);
    if (x2 >= s_lcd.width)
    {
        x2 = (uint16_t)(s_lcd.width - 1U);
    }
    if (y2 >= s_lcd.height)
    {
        y2 = (uint16_t)(s_lcd.height - 1U);
    }

    lcd_draw_line(x, y, x2, y, color);
    lcd_draw_line(x, y2, x2, y2, color);
    lcd_draw_line(x, y, x, y2, color);
    lcd_draw_line(x2, y, x2, y2, color);
}

void LCD_Port_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t color, uint8_t large)
{
    LCD_Font_t font = (large != 0U) ? LCD_FONT_24 : LCD_FONT_16;
    uint16_t width = (x < s_lcd.width) ? (uint16_t)(s_lcd.width - x) : 0U;
    uint16_t height = (large != 0U) ? 36U : 20U;

    if ((text == NULL) || (width == 0U))
    {
        return;
    }

    lcd_show_string(x, y, width, height, text, font, color);
}

void LCD_Port_SetBacklight(uint8_t percent)
{
    uint16_t pwm;

    pwm = (uint16_t)(((uint32_t)percent * 255U) / 100U);
    lcd_fmc_write_cmd(0xBEU);
    lcd_fmc_write_dat(0x05U);
    lcd_fmc_write_dat(pwm);
    lcd_fmc_write_dat(0x01U);
    lcd_fmc_write_dat(0xFFU);
    lcd_fmc_write_dat(0x00U);
    lcd_fmc_write_dat(0x00U);
}

uint16_t LCD_Port_GetWidth(void)
{
    return s_lcd.width;
}

uint16_t LCD_Port_GetHeight(void)
{
    return s_lcd.height;
}

uint8_t LCD_Port_GetDisplayDir(void)
{
    return s_lcd.disp_dir;
}

uint8_t LCD_Port_IsReady(void)
{
    return s_lcd.ready;
}
