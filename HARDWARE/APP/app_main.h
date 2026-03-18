#ifndef APP_MAIN_H
#define APP_MAIN_H

#include "l610_mqtt.h"
#include "relay.h"

typedef struct
{
    uint8_t mqtt_ready;
    L610_MQTT_State_t mqtt_state;
    RelayState_t relay1_state;
    RelayState_t relay2_state;
    uint32_t tick;
} App_RuntimeStatus_t;

void App_MainInit(void);
void App_MainTask(void);
uint8_t App_MQTTIsReady(void);
void App_GetRuntimeStatus(App_RuntimeStatus_t *status);
L610_MQTT_Status_t App_RequestStatusSync(void);

#endif
