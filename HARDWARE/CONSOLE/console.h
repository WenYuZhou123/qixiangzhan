#ifndef __CONSOLE_H
#define __CONSOLE_H

#include "main.h"

void Console_Init(void);
void Console_Task(void);
uint8_t Console_IsBackgroundPauseActive(void);
void Console_OnUartRxCplt(UART_HandleTypeDef *huart);
void Console_OnUartError(UART_HandleTypeDef *huart);

#endif
