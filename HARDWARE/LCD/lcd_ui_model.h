#ifndef LCD_UI_MODEL_H
#define LCD_UI_MODEL_H

#include "device_status.h"

typedef enum
{
    LCD_PAGE_OVERVIEW = 0,
    LCD_PAGE_PAD,
    LCD_PAGE_WEATHER,
    LCD_PAGE_DEBUG,
    LCD_PAGE_COUNT
} LCD_Page_t;

typedef struct
{
    App_DeviceStatus_t status;
} LCD_UI_Model_t;

void LCD_UI_ModelUpdate(LCD_UI_Model_t *model, const App_DeviceStatus_t *status);

#endif
