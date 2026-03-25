#include "lcd_ui_model.h"

#include <string.h>

static void lcd_ui_copy_text(char *dst, const char *src, uint32_t size)
{
    if ((dst == 0) || (size == 0U))
    {
        return;
    }

    if (src == 0)
    {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, size - 1U);
    dst[size - 1U] = '\0';
}

void LCD_UI_ModelUpdate(LCD_UI_Model_t *model, const App_DeviceStatus_t *status)
{
    if ((model == 0) || (status == 0))
    {
        return;
    }

    memcpy(&model->status, status, sizeof(model->status));
}

void LCD_UI_ModelSetFeedback(LCD_UI_Model_t *model, uint8_t busy, const char *message)
{
    if (model == 0)
    {
        return;
    }

    model->control_busy = busy;
    lcd_ui_copy_text(model->control_message, message, sizeof(model->control_message));
}
