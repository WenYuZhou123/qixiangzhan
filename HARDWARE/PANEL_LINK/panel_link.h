#ifndef __PANEL_LINK_H
#define __PANEL_LINK_H

#include <stdint.h>

typedef struct
{
    uint8_t online;
    uint8_t waiting;
    uint8_t last_sequence;
    uint8_t last_operation;
    uint8_t last_command;
    uint8_t last_command_result;
    uint32_t tx_frames;
    uint32_t rx_frames;
    uint32_t crc_errors;
    uint32_t timeout_count;
    uint32_t last_response_tick;
} PanelLink_Status_t;

void PanelLink_Init(void);
void PanelLink_Task(void);
uint8_t PanelLink_IsTransactionActive(void);
void PanelLink_GetStatus(PanelLink_Status_t *status);

#endif
