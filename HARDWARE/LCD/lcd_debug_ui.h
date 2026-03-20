#ifndef LCD_DEBUG_UI_H
#define LCD_DEBUG_UI_H

#include <stdint.h>

void LCD_DebugUI_Init(void);
void LCD_DebugUI_Task(void);
uint32_t LCD_DebugUI_GetRefreshTick(void);
uint32_t LCD_DebugUI_GetRefreshCount(void);

#endif
