#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "panel_protocol.h"
#include "sensor_fusion.h"

typedef enum {
    AI_REQUEST_IDLE = 0,
    AI_REQUEST_WAITING_NETWORK,
    AI_REQUEST_IN_PROGRESS,
    AI_REQUEST_READY,
    AI_REQUEST_ERROR,
} ai_request_status_t;

typedef struct {
    ai_request_status_t request_status;
    sensor_fusion_result_t fusion;
    panel_command_t recommended_command;
    bool requires_confirmation;
    uint32_t request_id;
    uint32_t snapshot_sequence;
    uint32_t updated_at_ms;
    char summary[160];
    char reasons[3][112];
    char error[112];
} ai_state_snapshot_t;

void ai_state_init(void);
void ai_state_begin(uint32_t snapshot_sequence, sensor_fusion_result_t fusion);
void ai_state_set_result(sensor_fusion_result_t fusion, panel_command_t command,
                         bool requires_confirmation, const char *summary,
                         const char *const reasons[3]);
void ai_state_set_error(sensor_fusion_result_t fusion, const char *error);
void ai_state_get_snapshot(ai_state_snapshot_t *snapshot);
const char *ai_state_status_name(ai_request_status_t status);
