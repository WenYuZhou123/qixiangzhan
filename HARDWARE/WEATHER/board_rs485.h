#ifndef BOARD_RS485_H
#define BOARD_RS485_H

#include "main.h"

HAL_StatusTypeDef BoardRS485_Init(void);
HAL_StatusTypeDef BoardRS485_Transmit(UART_HandleTypeDef *huart,
                                      const uint8_t *data,
                                      uint16_t len,
                                      uint32_t timeout_ms);
uint8_t BoardRS485_IsReady(void);

#endif
