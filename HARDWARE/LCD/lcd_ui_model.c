#include "lcd_ui_model.h"
#include <string.h>

void LCD_UI_ModelUpdate(LCD_UI_Model_t *model, const App_DeviceStatus_t *status)
{
    if ((model == 0) || (status == 0))
    {
        return;
    }

    memcpy(&model->status, status, sizeof(model->status));
}
