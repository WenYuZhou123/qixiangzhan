#include "lcd_debug_ui.h"

#include "app_main.h"
#include "key.h"
#include "lcd_port.h"
#include "lcd_touch.h"
#include "lcd_ui_model.h"
#include "lcd_ui_pages.h"

static LCD_Page_t s_page = LCD_PAGE_OVERVIEW;
static LCD_UI_Model_t s_model;
static uint32_t s_last_refresh_tick = 0U;
static uint8_t s_touch_active = 0U;
static uint8_t s_touch_tab_handled = 0U;
static uint16_t s_touch_start_x = 0U;
static uint16_t s_touch_start_y = 0U;
static uint16_t s_touch_last_x = 0U;
static uint16_t s_touch_last_y = 0U;

static uint16_t lcd_abs_diff_u16(uint16_t a, uint16_t b)
{
    return (a >= b) ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

static uint8_t lcd_hit_test_tab(uint16_t x, uint16_t y, LCD_Page_t *page)
{
    uint16_t step;
    uint16_t local_x;
    uint8_t index;

    if ((page == 0) ||
        (y < LCD_UI_TAB_Y) ||
        (y >= (LCD_UI_TAB_Y + LCD_UI_TAB_H)) ||
        (x < LCD_UI_TAB_X))
    {
        return 0U;
    }

    step = (uint16_t)(LCD_UI_TAB_W + LCD_UI_TAB_GAP);
    local_x = (uint16_t)(x - LCD_UI_TAB_X);
    index = (uint8_t)(local_x / step);
    if (index >= LCD_PAGE_COUNT)
    {
        return 0U;
    }

    if ((local_x % step) >= LCD_UI_TAB_W)
    {
        return 0U;
    }

    *page = (LCD_Page_t)index;
    return 1U;
}

void LCD_DebugUI_Init(void)
{
    LCD_Port_Init();
    LCD_Touch_Init();
    LCD_Port_SetBacklight(100U);
    s_page = LCD_PAGE_OVERVIEW;
    s_last_refresh_tick = 0U;
}

void LCD_DebugUI_Task(void)
{
    App_DeviceStatus_t status;
    LCD_TouchState_t touch;
    LCD_Page_t touched_page;
    uint8_t key_pressed = Key_Scan();

    if (key_pressed != 0U)
    {
        s_page = (LCD_Page_t)((s_page + 1U) % LCD_PAGE_COUNT);
    }

    if (LCD_Touch_Read(&touch) != 0U && touch.pressed != 0U)
    {
        if (s_touch_active == 0U)
        {
            s_touch_active = 1U;
            s_touch_tab_handled = 0U;
            s_touch_start_x = touch.x;
            s_touch_start_y = touch.y;
            s_touch_last_x = touch.x;
            s_touch_last_y = touch.y;

            if (lcd_hit_test_tab(touch.x, touch.y, &touched_page) != 0U)
            {
                s_page = touched_page;
                s_touch_tab_handled = 1U;
            }
        }
        else
        {
            s_touch_last_x = touch.x;
            s_touch_last_y = touch.y;
        }
    }
    else if (s_touch_active != 0U)
    {
        uint16_t dx = lcd_abs_diff_u16(s_touch_last_x, s_touch_start_x);
        uint16_t dy = lcd_abs_diff_u16(s_touch_last_y, s_touch_start_y);

        if ((s_touch_tab_handled == 0U) && (dx > 80U) && (dx > dy))
        {
            if (s_touch_last_x < s_touch_start_x)
            {
                s_page = (LCD_Page_t)((s_page + 1U) % LCD_PAGE_COUNT);
            }
            else
            {
                s_page = (LCD_Page_t)((s_page + LCD_PAGE_COUNT - 1U) % LCD_PAGE_COUNT);
            }
        }

        s_touch_active = 0U;
        s_touch_tab_handled = 0U;
    }

    if ((HAL_GetTick() - s_last_refresh_tick) < 150U)
    {
        return;
    }
    s_last_refresh_tick = HAL_GetTick();

    App_FillDeviceStatus(&status);
    LCD_UI_ModelUpdate(&s_model, &status);
    LCD_UI_RenderPage(s_page, &s_model);
}
