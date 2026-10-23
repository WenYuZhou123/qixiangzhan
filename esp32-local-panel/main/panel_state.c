#include "panel_state.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static panel_state_snapshot_t s_state;
static SemaphoreHandle_t s_state_lock;

static uint16_t panel_state_read_u16(const uint8_t *data)
{
    return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8U);
}

static uint32_t panel_state_read_u32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
}

static void panel_state_lock(void)
{
    if (s_state_lock != NULL) {
        (void)xSemaphoreTake(s_state_lock, portMAX_DELAY);
    }
}

static void panel_state_unlock(void)
{
    if (s_state_lock != NULL) {
        (void)xSemaphoreGive(s_state_lock);
    }
}

void panel_state_init(void)
{
    s_state_lock = xSemaphoreCreateMutex();
    s_state = (panel_state_snapshot_t){0};
}

void panel_state_mark_received(uint8_t sequence, uint8_t operation)
{
    panel_state_lock();
    s_state.h743_online = true;
    s_state.last_sequence = sequence;
    s_state.last_operation = operation;
    s_state.last_poll_tick = (uint32_t)(esp_timer_get_time() / 1000LL);
    s_state.rx_frames++;
    panel_state_unlock();
}

void panel_state_mark_sent(void)
{
    panel_state_lock();
    s_state.tx_frames++;
    panel_state_unlock();
}

void panel_state_mark_crc_error(void)
{
    panel_state_lock();
    s_state.crc_errors++;
    panel_state_unlock();
}

void panel_state_mark_format_error(void)
{
    panel_state_lock();
    s_state.format_errors++;
    panel_state_unlock();
}

void panel_state_request_ping(void)
{
    panel_state_lock();
    s_state.ping_requested = true;
    s_state.ping_acknowledged = false;
    panel_state_unlock();
}

void panel_state_request_command(panel_command_t command)
{
    if (command <= PANEL_COMMAND_NONE || command > PANEL_COMMAND_QUERY_STATUS) {
        return;
    }

    panel_state_lock();
    s_state.command_request_id++;
    if (s_state.command_request_id == 0U) {
        s_state.command_request_id = 1U;
    }
    s_state.pending_command = command;
    s_state.last_command_result = PANEL_COMMAND_RESULT_NONE;
    s_state.command_timed_out = false;
    panel_state_unlock();
}

void panel_state_build_response(uint8_t response_payload[2])
{
    if (response_payload == NULL) {
        return;
    }

    panel_state_lock();
    response_payload[0] = s_state.ping_requested ? PANEL_PROTOCOL_FLAG_PING_REQUEST : 0U;
    response_payload[1] = (uint8_t)s_state.pending_command;
    panel_state_unlock();
}

void panel_state_mark_ping_acknowledged(void)
{
    panel_state_lock();
    s_state.ping_requested = false;
    s_state.ping_acknowledged = true;
    panel_state_unlock();
}

bool panel_state_apply_status(const uint8_t *payload, uint8_t payload_length)
{
    uint8_t flags;
    panel_command_t command;
    panel_command_result_t result;

    if (payload == NULL || payload_length != PANEL_PROTOCOL_STATUS_PAYLOAD_SIZE ||
        payload[0] != PANEL_PROTOCOL_STATUS_VERSION) {
        return false;
    }

    flags = payload[1];
    command = (panel_command_t)payload[2];
    result = (panel_command_result_t)payload[3];

    panel_state_lock();
    s_state.status_valid = true;
    s_state.relay1_on = (flags & PANEL_STATUS_FLAG_RELAY1_ON) != 0U;
    s_state.relay2_on = (flags & PANEL_STATUS_FLAG_RELAY2_ON) != 0U;
    s_state.wind_online = (flags & PANEL_STATUS_FLAG_WIND_ONLINE) != 0U;
    s_state.air_online = (flags & PANEL_STATUS_FLAG_AIR_ONLINE) != 0U;
    s_state.rain_online = (flags & PANEL_STATUS_FLAG_RAIN_ONLINE) != 0U;
    s_state.rain_detected = (flags & PANEL_STATUS_FLAG_RAIN_DETECTED) != 0U;
    s_state.last_command = command;
    s_state.last_command_result = result;
    s_state.device_uptime_ms = panel_state_read_u32(&payload[4]);
    s_state.wind_speed_mps = (float)panel_state_read_u16(&payload[8]) / 10.0f;
    s_state.wind_direction_deg = (float)panel_state_read_u16(&payload[10]) / 10.0f;
    s_state.temperature_c = (float)(int16_t)panel_state_read_u16(&payload[12]) / 100.0f;
    s_state.humidity_rh = (float)panel_state_read_u16(&payload[14]) / 10.0f;
    s_state.rain_value = (float)panel_state_read_u16(&payload[16]) / 10.0f;
    s_state.pm25_ugm3 = (float)panel_state_read_u16(&payload[18]) / 10.0f;
    s_state.pm10_ugm3 = (float)panel_state_read_u16(&payload[20]) / 10.0f;
    s_state.co2_ppm = (float)panel_state_read_u16(&payload[22]);
    s_state.tvoc_mg_m3 = (float)panel_state_read_u16(&payload[24]) / 1000.0f;
    s_state.ch2o_mg_m3 = (float)panel_state_read_u16(&payload[26]) / 1000.0f;
    s_state.wind_speed_raw = panel_state_read_u16(&payload[28]);
    s_state.wind_direction_raw = panel_state_read_u16(&payload[30]);
    s_state.rain_raw = panel_state_read_u16(&payload[32]);
    if (s_state.pending_command == command && result != PANEL_COMMAND_RESULT_NONE) {
        s_state.completed_command = command;
        s_state.command_completion_id = s_state.command_request_id;
        s_state.command_timed_out = false;
        s_state.pending_command = PANEL_COMMAND_NONE;
    }
    panel_state_unlock();
    return true;
}

void panel_state_get_snapshot(panel_state_snapshot_t *snapshot)
{
    uint32_t now;

    if (snapshot == NULL) {
        return;
    }

    panel_state_lock();
    now = (uint32_t)(esp_timer_get_time() / 1000LL);
    s_state.last_update_age_ms = now - s_state.last_poll_tick;
    if (s_state.h743_online &&
        s_state.last_update_age_ms > CONFIG_PANEL_LINK_OFFLINE_TIMEOUT_MS) {
        s_state.h743_online = false;
        s_state.timeout_errors++;
        if (s_state.pending_command != PANEL_COMMAND_NONE) {
            s_state.completed_command = s_state.pending_command;
            s_state.command_completion_id = s_state.command_request_id;
            s_state.last_command = s_state.pending_command;
            s_state.last_command_result = PANEL_COMMAND_RESULT_ERROR;
            s_state.command_timed_out = true;
            s_state.pending_command = PANEL_COMMAND_NONE;
        }
    }
    *snapshot = s_state;
    panel_state_unlock();
}
