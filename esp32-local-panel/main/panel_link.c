#include "panel_link.h"

#include <string.h>

#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "panel_protocol.h"
#include "panel_state.h"

static const char *TAG = "panel_link";
static uint8_t s_rx_buffer[PANEL_PROTOCOL_MAX_FRAME * 2U];
static size_t s_rx_length;

static uint16_t panel_link_crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFU;

    for (size_t index = 0; index < length; index++) {
        crc ^= data[index];
        for (uint8_t bit = 0; bit < 8U; bit++) {
            crc = (crc & 1U) != 0U ? (uint16_t)((crc >> 1U) ^ 0xA001U) : (uint16_t)(crc >> 1U);
        }
    }
    return crc;
}

static void panel_link_send_response(uint8_t sequence, uint8_t operation)
{
    uint8_t frame[PANEL_PROTOCOL_MAX_FRAME];
    uint8_t payload[2];
    uint16_t crc;
    size_t length = 0U;

    frame[length++] = PANEL_PROTOCOL_ADDRESS;
    frame[length++] = PANEL_PROTOCOL_FUNCTION;
    frame[length++] = sequence;
    frame[length++] = operation;
    panel_state_build_response(payload);
    frame[length++] = sizeof(payload);
    frame[length++] = payload[0];
    frame[length++] = payload[1];
    crc = panel_link_crc16(frame, length);
    frame[length++] = (uint8_t)(crc & 0xFFU);
    frame[length++] = (uint8_t)(crc >> 8U);

    if (uart_write_bytes(CONFIG_PANEL_RS485_UART_NUM, (const char *)frame, length) == (int)length) {
        (void)uart_wait_tx_done(CONFIG_PANEL_RS485_UART_NUM, pdMS_TO_TICKS(100));
        panel_state_mark_sent();
    } else {
        ESP_LOGW(TAG, "RS485 response write failed");
    }
}

static void panel_link_handle_frame(const uint8_t *frame, size_t length)
{
    uint8_t sequence = frame[2];
    uint8_t operation = frame[3];

    panel_state_mark_received(sequence, operation);
    if (operation == PANEL_PROTOCOL_OP_POLL) {
        panel_link_send_response(sequence, PANEL_PROTOCOL_OP_READY);
    } else if (operation == PANEL_PROTOCOL_OP_STATUS) {
        if (!panel_state_apply_status(&frame[5], frame[4])) {
            panel_state_mark_format_error();
            panel_link_send_response(sequence, PANEL_PROTOCOL_OP_ERROR);
            return;
        }
        panel_link_send_response(sequence, PANEL_PROTOCOL_OP_READY);
    } else if (operation == PANEL_PROTOCOL_OP_PING) {
        panel_state_mark_ping_acknowledged();
        panel_link_send_response(sequence, PANEL_PROTOCOL_OP_ACK);
    } else {
        panel_link_send_response(sequence, PANEL_PROTOCOL_OP_ERROR);
    }

    ESP_LOGD(TAG, "frame op=%u length=%u", operation, (unsigned)length);
}

static void panel_link_parse_rx(void)
{
    while (s_rx_length >= 5U) {
        size_t frame_length;
        uint16_t expected_crc;
        uint16_t received_crc;

        if (s_rx_buffer[0] != PANEL_PROTOCOL_ADDRESS || s_rx_buffer[1] != PANEL_PROTOCOL_FUNCTION) {
            memmove(s_rx_buffer, s_rx_buffer + 1U, --s_rx_length);
            panel_state_mark_format_error();
            continue;
        }

        if (s_rx_buffer[4] > PANEL_PROTOCOL_MAX_PAYLOAD) {
            memmove(s_rx_buffer, s_rx_buffer + 1U, --s_rx_length);
            panel_state_mark_format_error();
            continue;
        }

        frame_length = 7U + s_rx_buffer[4];
        if (s_rx_length < frame_length) {
            return;
        }

        expected_crc = panel_link_crc16(s_rx_buffer, frame_length - 2U);
        received_crc = (uint16_t)s_rx_buffer[frame_length - 2U] |
                       (uint16_t)((uint16_t)s_rx_buffer[frame_length - 1U] << 8U);
        if (expected_crc == received_crc) {
            panel_link_handle_frame(s_rx_buffer, frame_length);
        } else {
            panel_state_mark_crc_error();
        }

        s_rx_length -= frame_length;
        if (s_rx_length > 0U) {
            memmove(s_rx_buffer, s_rx_buffer + frame_length, s_rx_length);
        }
    }
}

static void panel_link_task(void *argument)
{
    uint8_t incoming[32];

    (void)argument;
    while (true) {
        int received = uart_read_bytes(CONFIG_PANEL_RS485_UART_NUM,
                                       incoming,
                                       sizeof(incoming),
                                       pdMS_TO_TICKS(50));
        if (received <= 0) {
            continue;
        }

        for (int index = 0; index < received; index++) {
            if (s_rx_length == sizeof(s_rx_buffer)) {
                memmove(s_rx_buffer, s_rx_buffer + 1U, --s_rx_length);
                panel_state_mark_format_error();
            }
            s_rx_buffer[s_rx_length++] = incoming[index];
        }
        panel_link_parse_rx();
    }
}

esp_err_t panel_link_init(void)
{
    const uart_config_t configuration = {
        .baud_rate = CONFIG_PANEL_RS485_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(uart_driver_install(CONFIG_PANEL_RS485_UART_NUM,
                                            CONFIG_PANEL_RS485_RX_BUFFER_SIZE,
                                            CONFIG_PANEL_RS485_TX_BUFFER_SIZE,
                                            0,
                                            NULL,
                                            0),
                        TAG,
                        "install UART driver");
    ESP_RETURN_ON_ERROR(uart_param_config(CONFIG_PANEL_RS485_UART_NUM, &configuration), TAG, "configure UART");
    ESP_RETURN_ON_ERROR(uart_set_pin(CONFIG_PANEL_RS485_UART_NUM,
                                     CONFIG_PANEL_RS485_TX_GPIO,
                                     CONFIG_PANEL_RS485_RX_GPIO,
                                     UART_PIN_NO_CHANGE,
                                     UART_PIN_NO_CHANGE),
                        TAG,
                        "assign RS485 pins");

    xTaskCreate(panel_link_task, "panel_link", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "RS485 listening on UART%d TX=%d RX=%d @%d",
             CONFIG_PANEL_RS485_UART_NUM,
             CONFIG_PANEL_RS485_TX_GPIO,
             CONFIG_PANEL_RS485_RX_GPIO,
             CONFIG_PANEL_RS485_BAUD_RATE);
    return ESP_OK;
}
