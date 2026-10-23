#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "panel_protocol.h"

typedef struct {
    bool h743_online;
    bool ping_requested;
    bool ping_acknowledged;
    bool status_valid;
    bool relay1_on;
    bool relay2_on;
    bool wind_online;
    bool air_online;
    bool rain_online;
    bool rain_detected;
    bool command_timed_out;
    uint8_t last_sequence;
    uint8_t last_operation;
    panel_command_t pending_command;
    panel_command_t completed_command;
    panel_command_t last_command;
    panel_command_result_t last_command_result;
    uint32_t command_request_id;
    uint32_t command_completion_id;
    uint32_t device_uptime_ms;
    uint32_t last_poll_tick;
    uint32_t last_update_age_ms;
    uint16_t wind_speed_raw;
    uint16_t wind_direction_raw;
    uint16_t rain_raw;
    float wind_speed_mps;
    float wind_direction_deg;
    float temperature_c;
    float humidity_rh;
    float rain_value;
    float pm25_ugm3;
    float pm10_ugm3;
    float co2_ppm;
    float tvoc_mg_m3;
    float ch2o_mg_m3;
    uint32_t tx_frames;
    uint32_t rx_frames;
    uint32_t crc_errors;
    uint32_t format_errors;
    uint32_t timeout_errors;
} panel_state_snapshot_t;

void panel_state_init(void);
void panel_state_mark_received(uint8_t sequence, uint8_t operation);
void panel_state_mark_sent(void);
void panel_state_mark_crc_error(void);
void panel_state_mark_format_error(void);
void panel_state_request_ping(void);
void panel_state_request_command(panel_command_t command);
void panel_state_build_response(uint8_t response_payload[2]);
void panel_state_mark_ping_acknowledged(void);
bool panel_state_apply_status(const uint8_t *payload, uint8_t payload_length);
void panel_state_get_snapshot(panel_state_snapshot_t *snapshot);
