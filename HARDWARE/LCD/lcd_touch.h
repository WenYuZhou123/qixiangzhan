#ifndef LCD_TOUCH_H
#define LCD_TOUCH_H

#include <stdint.h>

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint8_t pressed;
} LCD_TouchState_t;

void LCD_Touch_Init(void);
uint8_t LCD_Touch_Read(LCD_TouchState_t *state);

#endif
