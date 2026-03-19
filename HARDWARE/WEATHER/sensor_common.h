#ifndef __SENSOR_COMMON_H
#define __SENSOR_COMMON_H

#include <stdint.h>

typedef struct
{
    uint8_t online;
    uint32_t last_update_tick;
} SensorBase_t;

#endif
