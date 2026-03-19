#ifndef LCD_PORT_H
#define LCD_PORT_H

#include <stdint.h>

#define LCD_PORT_COLOR_BG        0x1B34U
#define LCD_PORT_COLOR_CARD      0xFFFFU
#define LCD_PORT_COLOR_TEXT      0x18C3U
#define LCD_PORT_COLOR_MUTED     0x7BEFU
#define LCD_PORT_COLOR_ACCENT    0x2D7FU
#define LCD_PORT_COLOR_SUCCESS   0x3D8CU
#define LCD_PORT_COLOR_WARN      0xFD20U
#define LCD_PORT_COLOR_DANGER    0xF145U

#define LCD_PORT_DISP_DIR_0      0U
#define LCD_PORT_DISP_DIR_90     1U
#define LCD_PORT_DISP_DIR_180    2U
#define LCD_PORT_DISP_DIR_270    3U

void LCD_Port_Init(void);
void LCD_Port_BeginFrame(void);
void LCD_Port_EndFrame(void);
void LCD_Port_Clear(uint16_t color);
void LCD_Port_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void LCD_Port_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void LCD_Port_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t color, uint8_t large);
void LCD_Port_SetBacklight(uint8_t percent);
uint16_t LCD_Port_GetWidth(void);
uint16_t LCD_Port_GetHeight(void);
uint8_t LCD_Port_GetDisplayDir(void);
uint8_t LCD_Port_IsReady(void);

#endif
