#ifndef LCD_UI_PAGES_H
#define LCD_UI_PAGES_H

#include "lcd_ui_model.h"

#define LCD_UI_SCREEN_WIDTH   800U
#define LCD_UI_SCREEN_HEIGHT  480U
#define LCD_UI_HEADER_HEIGHT  56U
#define LCD_UI_TAB_X          220U
#define LCD_UI_TAB_Y          8U
#define LCD_UI_TAB_W          132U
#define LCD_UI_TAB_H          40U
#define LCD_UI_TAB_GAP        8U

void LCD_UI_RenderPage(LCD_Page_t page, const LCD_UI_Model_t *model);

#endif
