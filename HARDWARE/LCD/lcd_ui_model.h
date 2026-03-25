#ifndef LCD_UI_MODEL_H
#define LCD_UI_MODEL_H

#include "device_status.h"

typedef enum
{
    LCD_PAGE_OVERVIEW = 0,
    LCD_PAGE_CONTROL,
    LCD_PAGE_WEATHER,
    LCD_PAGE_DEBUG,
    LCD_PAGE_COUNT
} LCD_Page_t;

typedef enum
{
    LCD_CONTROL_ACTION_NONE = 0,
    LCD_CONTROL_ACTION_R1_ON,
    LCD_CONTROL_ACTION_R1_OFF,
    LCD_CONTROL_ACTION_R2_ON,
    LCD_CONTROL_ACTION_R2_OFF,
    LCD_CONTROL_ACTION_ALL_ON,
    LCD_CONTROL_ACTION_ALL_OFF,
    LCD_CONTROL_ACTION_QUERY_STATUS
} LCD_ControlAction_t;

typedef struct
{
    App_DeviceStatus_t status;
    uint8_t control_busy;
    char control_message[APP_STATUS_TEXT_LEN];
} LCD_UI_Model_t;

void LCD_UI_ModelUpdate(LCD_UI_Model_t *model, const App_DeviceStatus_t *status);
void LCD_UI_ModelSetFeedback(LCD_UI_Model_t *model, uint8_t busy, const char *message);

#endif
