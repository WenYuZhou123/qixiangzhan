#ifndef __RELAY_H
#define __RELAY_H

#include "main.h"

typedef enum
{
    RELAY1 = 0,
    RELAY2
} RelayId_t;

typedef enum
{
    RELAY_OFF = 0,
    RELAY_ON  = 1
} RelayState_t;

void Relay_Init(void);
void Relay_On(RelayId_t id);
void Relay_Off(RelayId_t id);
void Relay_Toggle(RelayId_t id);
RelayState_t Relay_GetState(RelayId_t id);

void Relay_AllOn(void);
void Relay_AllOff(void);
void Relay_AllToggle(void);

#endif
