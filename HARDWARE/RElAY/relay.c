#include "relay.h"

#define RELAY1_GPIO_PORT    GPIOA
#define RELAY1_GPIO_PIN     GPIO_PIN_4

#define RELAY2_GPIO_PORT    GPIOA
#define RELAY2_GPIO_PIN     GPIO_PIN_5

static RelayState_t relay_state[2] = {RELAY_OFF, RELAY_OFF};

static GPIO_TypeDef* Relay_GetPort(RelayId_t id)
{
    if (id == RELAY1) return RELAY1_GPIO_PORT;
    else              return RELAY2_GPIO_PORT;
}

static uint16_t Relay_GetPin(RelayId_t id)
{
    if (id == RELAY1) return RELAY1_GPIO_PIN;
    else              return RELAY2_GPIO_PIN;
}

void Relay_Init(void)
{
    Relay_AllOff();
}

void Relay_On(RelayId_t id)
{
    HAL_GPIO_WritePin(Relay_GetPort(id), Relay_GetPin(id), GPIO_PIN_SET);
    relay_state[id] = RELAY_ON;
}

void Relay_Off(RelayId_t id)
{
    HAL_GPIO_WritePin(Relay_GetPort(id), Relay_GetPin(id), GPIO_PIN_RESET);
    relay_state[id] = RELAY_OFF;
}

void Relay_Toggle(RelayId_t id)
{
    HAL_GPIO_TogglePin(Relay_GetPort(id), Relay_GetPin(id));

    if (relay_state[id] == RELAY_OFF)
        relay_state[id] = RELAY_ON;
    else
        relay_state[id] = RELAY_OFF;
}

RelayState_t Relay_GetState(RelayId_t id)
{
    return relay_state[id];
}

void Relay_AllOn(void)
{
    Relay_On(RELAY1);
    Relay_On(RELAY2);
}

void Relay_AllOff(void)
{
    Relay_Off(RELAY1);
    Relay_Off(RELAY2);
}

void Relay_AllToggle(void)
{
    Relay_Toggle(RELAY1);
    Relay_Toggle(RELAY2);
}
