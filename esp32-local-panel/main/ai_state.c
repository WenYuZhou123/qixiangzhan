#include "ai_state.h"

#include <string.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static ai_state_snapshot_t s_state;
static SemaphoreHandle_t s_lock;

static void ai_state_copy(char *destination, size_t destination_size, const char *source)
{
    if (destination_size == 0U) {
        return;
    }
    strlcpy(destination, source != NULL ? source : "", destination_size);
}

static uint32_t ai_state_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

void ai_state_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_state = (ai_state_snapshot_t){.request_status = AI_REQUEST_IDLE};
}

void ai_state_begin(uint32_t snapshot_sequence, sensor_fusion_result_t fusion)
{
    (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    s_state.request_id++;
    if (s_state.request_id == 0U) {
        s_state.request_id = 1U;
    }
    s_state.request_status = AI_REQUEST_IN_PROGRESS;
    s_state.fusion = fusion;
    s_state.snapshot_sequence = snapshot_sequence;
    s_state.recommended_command = PANEL_COMMAND_NONE;
    s_state.requires_confirmation = true;
    s_state.error[0] = '\0';
    s_state.updated_at_ms = ai_state_now_ms();
    (void)xSemaphoreGive(s_lock);
}

void ai_state_set_result(sensor_fusion_result_t fusion, panel_command_t command,
                         bool requires_confirmation, const char *summary,
                         const char *const reasons[3])
{
    (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    s_state.request_status = AI_REQUEST_READY;
    s_state.fusion = fusion;
    s_state.recommended_command = command;
    s_state.requires_confirmation = requires_confirmation;
    ai_state_copy(s_state.summary, sizeof(s_state.summary), summary);
    for (size_t index = 0; index < 3U; index++) {
        ai_state_copy(s_state.reasons[index], sizeof(s_state.reasons[index]), reasons[index]);
    }
    s_state.error[0] = '\0';
    s_state.updated_at_ms = ai_state_now_ms();
    (void)xSemaphoreGive(s_lock);
}

void ai_state_set_error(sensor_fusion_result_t fusion, const char *error)
{
    (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    s_state.request_status = AI_REQUEST_ERROR;
    s_state.fusion = fusion;
    s_state.recommended_command = PANEL_COMMAND_NONE;
    s_state.requires_confirmation = true;
    ai_state_copy(s_state.error, sizeof(s_state.error), error);
    s_state.updated_at_ms = ai_state_now_ms();
    (void)xSemaphoreGive(s_lock);
}

void ai_state_get_snapshot(ai_state_snapshot_t *snapshot)
{
    if (snapshot == NULL || s_lock == NULL) {
        return;
    }
    (void)xSemaphoreTake(s_lock, portMAX_DELAY);
    *snapshot = s_state;
    (void)xSemaphoreGive(s_lock);
}

const char *ai_state_status_name(ai_request_status_t status)
{
    switch (status) {
    case AI_REQUEST_WAITING_NETWORK:
        return "waiting network";
    case AI_REQUEST_IN_PROGRESS:
        return "analyzing";
    case AI_REQUEST_READY:
        return "ready";
    case AI_REQUEST_ERROR:
        return "error";
    default:
        return "idle";
    }
}
