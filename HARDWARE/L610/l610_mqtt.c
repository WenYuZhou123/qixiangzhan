#include "l610_mqtt.h"
#include "device_status.h"
#include "protocol.h"
#include "relay.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MQTT_TX_QUEUE_DEPTH            8U
#define MQTT_PRIORITY_QUEUE_DEPTH      8U
#define MQTT_CMD_QUEUE_DEPTH           8U
#define MQTT_RECENT_CMD_DEPTH          16U
#define MQTT_CMD_ID_BUF_SIZE           48U
#define MQTT_CMD_NAME_BUF_SIZE         32U
#define MQTT_TIMESTAMP_BUF_SIZE        32U
#define MQTT_PUBLISH_ACK_TIMEOUT_MS    10000U
#define MQTT_PUBLISH_QOS0_SETTLE_MS    300U
#define MQTT_POST_RX_GUARD_MS          150U
#define MQTT_COMMAND_FAST_WINDOW_MS    1200U
#define MQTT_STATUS_KEEPALIVE_MS       MQTT_KEEPALIVE_STATUS_INTERVAL_MS
#define MQTT_SUBSCRIBE_REFRESH_DELAY_MS 1500U
#define MQTT_RX_POLL_TIMEOUT_MS        5U
#define MQTT_RX_ASYNC_TIMEOUT_MS       2U
#define MQTT_RX_INTERBYTE_TIMEOUT_MS   800U
#define MQTT_DEBUG_VERBOSE_IO          MQTT_FEATURE_VERBOSE_LOG
#define MQTT_DEBUG_VERBOSE_PAYLOAD     0U
#define MQTT_DEBUG_COMMAND_TRACE       0U
#define MQTT_DEBUG_WILDCARD_TOPIC      MQTT_TOPIC_DEBUG_ALL

typedef struct
{
    char topic[MQTT_TOPIC_BUF_SIZE];
    char payload[MQTT_JSON_BUF_SIZE];
    uint8_t qos;
    uint8_t retain;
} MQTT_TxQueueEntry_t;

typedef struct
{
    char protocol_cmd[MQTT_CMD_BUF_SIZE];
    char request_cmd[MQTT_CMD_NAME_BUF_SIZE];
    char msg_id[MQTT_CMD_ID_BUF_SIZE];
    char device_id[MQTT_CLIENT_ID_BUF_SIZE];
    char operator_name[MQTT_USER_BUF_SIZE];
    int value;
    uint8_t has_value;
} MQTT_CommandRequest_t;

static MQTT_CommandRequest_t mqtt_cmd_current;
static MQTT_CommandRequest_t mqtt_cmd_queue[MQTT_CMD_QUEUE_DEPTH];
static char mqtt_cmd_buf[MQTT_CMD_BUF_SIZE];
static char mqtt_line_buf[MQTT_AT_BUF_SIZE];
static char mqtt_last_line[MQTT_AT_BUF_SIZE];
static char mqtt_async_buf[MQTT_AT_BUF_SIZE];
static char mqtt_json_buf[MQTT_JSON_BUF_SIZE];
static char mqtt_cmd_tx_buf[MQTT_TX_CMD_BUF_SIZE];
static char mqtt_topic_tx_buf[MQTT_TOPIC_BUF_SIZE * 2];
static char mqtt_payload_tx_buf[MQTT_JSON_BUF_SIZE * 2];
static char mqtt_last_tx[MQTT_TX_CMD_BUF_SIZE];
static char mqtt_last_rx[MQTT_AT_BUF_SIZE];
static L610_MQTT_Config_t mqtt_config;
static L610_MQTT_Message_t mqtt_last_message;
static L610_MQTT_State_t mqtt_state = MQTT_STATE_IDLE;
static L610_MQTT_MessageCallback_t mqtt_message_callback = NULL;
static L610_MQTT_EventCallback_t mqtt_event_callback = NULL;
static uint8_t mqtt_cmd_pending = 0;
static uint8_t mqtt_cmd_queue_head = 0;
static uint8_t mqtt_cmd_queue_tail = 0;
static uint8_t mqtt_cmd_queue_count = 0;
static uint8_t mqtt_message_pending = 0;
static int mqtt_last_result_code = 0;
static uint16_t mqtt_async_len = 0;
static uint32_t mqtt_async_last_tick = 0;
static MQTT_TxQueueEntry_t mqtt_tx_queue[MQTT_TX_QUEUE_DEPTH];
static MQTT_TxQueueEntry_t mqtt_priority_queue[MQTT_PRIORITY_QUEUE_DEPTH];
static uint8_t mqtt_tx_head = 0;
static uint8_t mqtt_tx_tail = 0;
static uint8_t mqtt_tx_count = 0;
static uint8_t mqtt_priority_head = 0U;
static uint8_t mqtt_priority_tail = 0U;
static uint8_t mqtt_priority_count = 0U;
static uint8_t mqtt_tx_inflight = 0;
static uint8_t mqtt_tx_inflight_priority = 0U;
static uint32_t mqtt_tx_started_tick = 0;
static uint32_t mqtt_tx_ready_tick = 0;
static uint8_t mqtt_status_publish_requested = 1;
static uint8_t mqtt_status_snapshot_valid = 0;
static RelayState_t mqtt_status_last_relay1 = RELAY_OFF;
static RelayState_t mqtt_status_last_relay2 = RELAY_OFF;
static uint32_t mqtt_status_last_publish_tick = 0U;
static uint8_t mqtt_online_publish_requested = 1U;
static uint8_t mqtt_online_state = 1U;
static uint8_t mqtt_subscribe_refresh_pending = 0U;
static uint8_t mqtt_subscribe_refresh_done = 0U;
static uint32_t mqtt_subscribe_refresh_tick = 0U;
static uint8_t mqtt_status_priority_requested = 0U;
static uint32_t mqtt_command_fast_until = 0U;
static uint8_t mqtt_last_status_publish_ok = 0U;
static uint32_t mqtt_last_status_publish_tick = 0U;
static char mqtt_recent_msg_ids[MQTT_RECENT_CMD_DEPTH][MQTT_CMD_ID_BUF_SIZE];
static uint8_t mqtt_recent_msg_index = 0U;

static L610_MQTT_Status_t MQTT_WaitForKeywordOrOk(const char *keyword, uint32_t timeout_ms);
static int MQTT_FindKnownCommandToken(const char *text, char *cmd_out, uint16_t cmd_size);
static int MQTT_MIPCALLIsStaleActive(const char *line);
static L610_MQTT_Status_t MQTT_ResetIPSession(void);
static L610_MQTT_Status_t MQTT_QueuePriorityPublish(const char *topic, const char *payload, uint8_t qos, uint8_t retain);

static uint8_t MQTT_IsLongPublishCommand(const char *cmd)
{
    if (cmd == NULL)
    {
        return 0;
    }

    return (strstr(cmd, "AT+MQTTPUB=") != NULL) ? 1U : 0U;
}

static void MQTT_DebugPrint(const char *str)
{
    if (str != NULL)
    {
#if MQTT_FEATURE_VERBOSE_LOG
        HAL_UART_Transmit(&huart6, (uint8_t *)str, strlen(str), 1000);
#else
        (void)str;
#endif
    }
}

static void MQTT_DebugPrintLine(const char *prefix, const char *value)
{
    MQTT_DebugPrint(prefix != NULL ? prefix : "");
    MQTT_DebugPrint(value != NULL ? value : "");
    MQTT_DebugPrint("\r\n");
}

static void MQTT_SafeCopy(char *dst, uint16_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0)
    {
        return;
    }

    if (src == NULL)
    {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void MQTT_TrimRight(char *str)
{
    uint16_t len;

    if (str == NULL)
    {
        return;
    }

    len = (uint16_t)strlen(str);
    while (len > 0)
    {
        if (str[len - 1] == '\r' || str[len - 1] == '\n' || str[len - 1] == ' ' || str[len - 1] == '\t')
        {
            str[len - 1] = '\0';
            len--;
        }
        else
        {
            break;
        }
    }
}

static void MQTT_ClearLastMessageInternal(void)
{
    memset(&mqtt_last_message, 0, sizeof(mqtt_last_message));
    mqtt_message_pending = 0;
}

static int MQTT_HasNonZeroIP(const char *line)
{
    if (line == NULL)
    {
        return 0;
    }

    if (strstr(line, "0.0.0.0") != NULL)
    {
        return 0;
    }

    return strchr(line, '.') != NULL;
}

static int MQTT_MIPCALLHasValidIP(const char *line)
{
    const char *p;
    char ip[32];
    uint16_t len;

    if (line == NULL)
    {
        return 0;
    }

    p = strchr(line, ':');
    if (p == NULL)
    {
        return 0;
    }

    p++;
    while (*p == ' ' || *p == '\t')
    {
        p++;
    }

    while (*p >= '0' && *p <= '9')
    {
        p++;
    }

    if (*p == ',')
    {
        p++;
    }

    while (*p == ' ' || *p == '\t' || *p == '"')
    {
        p++;
    }

    len = 0;
    while (p[len] != '\0' &&
           p[len] != '"' &&
           p[len] != '\r' &&
           p[len] != '\n' &&
           p[len] != ',')
    {
        if (len >= sizeof(ip) - 1)
        {
            return 0;
        }
        ip[len] = p[len];
        len++;
    }
    ip[len] = '\0';

    if (len == 0)
    {
        return 0;
    }

    if (strcmp(ip, "0.0.0.0") == 0)
    {
        return 0;
    }

    return strchr(ip, '.') != NULL;
}

static int MQTT_MIPCALLIsStaleActive(const char *line)
{
    if (line == NULL)
    {
        return 0;
    }

    return (strstr(line, "+MIPCALL: 1") != NULL && strstr(line, "0.0.0.0") != NULL) ? 1 : 0;
}

static void MQTT_SetLastLine(const char *line)
{
    MQTT_SafeCopy(mqtt_last_line, sizeof(mqtt_last_line), line);
    MQTT_SafeCopy(mqtt_last_rx, sizeof(mqtt_last_rx), line);
}

static void MQTT_SetLastTx(const char *cmd)
{
    MQTT_SafeCopy(mqtt_last_tx, sizeof(mqtt_last_tx), cmd);
    MQTT_TrimRight(mqtt_last_tx);
}

static void MQTT_LogTx(const char *cmd)
{
    MQTT_SetLastTx(cmd);
    if (MQTT_DEBUG_VERBOSE_IO != 0U && MQTT_IsLongPublishCommand(mqtt_last_tx) == 0U)
    {
        MQTT_DebugPrintLine("[MQTT][TX] ", mqtt_last_tx);
    }
}

static void MQTT_LogFailureContext(const char *stage)
{
    char result_buf[32];

    MQTT_DebugPrint(stage != NULL ? stage : "[MQTT] Failure context");
    MQTT_DebugPrint("\r\n");
    MQTT_DebugPrintLine("[MQTT] Last TX: ", mqtt_last_tx);
    MQTT_DebugPrintLine("[MQTT] Last RX: ", mqtt_last_rx);
    snprintf(result_buf, sizeof(result_buf), "%d", mqtt_last_result_code);
    MQTT_DebugPrintLine("[MQTT] Result: ", result_buf);
}

static void MQTT_EmitEvent(L610_MQTT_Event_t event, const char *detail)
{
    if (mqtt_event_callback != NULL)
    {
        mqtt_event_callback(event, detail);
    }
}

static void MQTT_EscapeJson(const char *src, char *dst, uint16_t dst_size)
{
    uint16_t used;
    char ch;

    if (dst == NULL || dst_size == 0)
    {
        return;
    }

    dst[0] = '\0';
    if (src == NULL)
    {
        return;
    }

    used = 0;
    while (*src != '\0' && used < dst_size - 1)
    {
        ch = *src++;
        if ((ch == '\\' || ch == '"') && used + 2 < dst_size)
        {
            dst[used++] = '\\';
            dst[used++] = ch;
        }
        else if (ch == '\r' && used + 2 < dst_size)
        {
            dst[used++] = '\\';
            dst[used++] = 'r';
        }
        else if (ch == '\n' && used + 2 < dst_size)
        {
            dst[used++] = '\\';
            dst[used++] = 'n';
        }
        else if (ch == '\t' && used + 2 < dst_size)
        {
            dst[used++] = '\\';
            dst[used++] = 't';
        }
        else
        {
            dst[used++] = ch;
        }
    }

    dst[used] = '\0';
}

static void MQTT_EscapeForAT(const char *src, char *dst, uint16_t dst_size)
{
    uint16_t used;
    char ch;

    if (dst == NULL || dst_size == 0)
    {
        return;
    }

    dst[0] = '\0';
    if (src == NULL)
    {
        return;
    }

    used = 0;
    while (*src != '\0' && used < dst_size - 1)
    {
        ch = *src++;
        if ((ch == '\\' || ch == '"') && used + 2 < dst_size)
        {
            dst[used++] = '\\';
            dst[used++] = ch;
        }
        else if (ch == '\r' && used + 2 < dst_size)
        {
            dst[used++] = '\\';
            dst[used++] = 'r';
        }
        else if (ch == '\n' && used + 2 < dst_size)
        {
            dst[used++] = '\\';
            dst[used++] = 'n';
        }
        else
        {
            dst[used++] = ch;
        }
    }

    dst[used] = '\0';
}

static void MQTT_RequestStatusPublishInternal(void)
{
    mqtt_status_publish_requested = 1U;
}

static void MQTT_RequestOnlinePublishInternal(uint8_t online)
{
    mqtt_online_state = online;
    mqtt_online_publish_requested = 1U;
}

static void MQTT_FormatTimestamp(char *out_buf, uint16_t out_size)
{
    if (out_buf == NULL || out_size == 0U)
    {
        return;
    }

    snprintf(out_buf, out_size, "%lu", (unsigned long)HAL_GetTick());
}

static void MQTT_ClearCommandRequest(MQTT_CommandRequest_t *request)
{
    if (request != NULL)
    {
        memset(request, 0, sizeof(*request));
    }
}

static uint8_t MQTT_FindRecentMsgId(const char *msg_id)
{
    uint8_t i;

    if (msg_id == NULL || msg_id[0] == '\0')
    {
        return 0U;
    }

    for (i = 0U; i < MQTT_RECENT_CMD_DEPTH; i++)
    {
        if (strcmp(mqtt_recent_msg_ids[i], msg_id) == 0)
        {
            return 1U;
        }
    }

    return 0U;
}

static void MQTT_RememberRecentMsgId(const char *msg_id)
{
    if (msg_id == NULL || msg_id[0] == '\0')
    {
        return;
    }

    MQTT_SafeCopy(mqtt_recent_msg_ids[mqtt_recent_msg_index],
                  sizeof(mqtt_recent_msg_ids[mqtt_recent_msg_index]),
                  msg_id);
    mqtt_recent_msg_index++;
    if (mqtt_recent_msg_index >= MQTT_RECENT_CMD_DEPTH)
    {
        mqtt_recent_msg_index = 0U;
    }
}

static void MQTT_ResetStatusTracking(void)
{
    mqtt_status_publish_requested = 1U;
    mqtt_status_snapshot_valid = 0U;
    mqtt_status_last_relay1 = RELAY_OFF;
    mqtt_status_last_relay2 = RELAY_OFF;
    mqtt_status_last_publish_tick = 0U;
    mqtt_online_publish_requested = 1U;
    mqtt_online_state = 1U;
    mqtt_subscribe_refresh_pending = 0U;
    mqtt_subscribe_refresh_done = 0U;
    mqtt_subscribe_refresh_tick = 0U;
    mqtt_status_priority_requested = 0U;
    mqtt_command_fast_until = 0U;
}

static void MQTT_ClearCommandQueue(void)
{
    memset(mqtt_cmd_queue, 0, sizeof(mqtt_cmd_queue));
    MQTT_ClearCommandRequest(&mqtt_cmd_current);
    mqtt_cmd_queue_head = 0U;
    mqtt_cmd_queue_tail = 0U;
    mqtt_cmd_queue_count = 0U;
    mqtt_cmd_pending = 0U;
    memset(mqtt_recent_msg_ids, 0, sizeof(mqtt_recent_msg_ids));
    mqtt_recent_msg_index = 0U;
}

static const char *MQTT_GetProtocolStatusString(ProtocolStatus_t status)
{
    switch (status)
    {
    case PROTOCOL_OK:
        return "OK";
    case PROTOCOL_UNKNOWN_CMD:
        return "UNKNOWN_CMD";
    case PROTOCOL_INVALID_PARAM:
        return "INVALID_PARAM";
    case PROTOCOL_EXEC_ERROR:
        return "EXEC_ERROR";
    default:
        return "UNKNOWN";
    }
}

static void MQTT_LogRelaySnapshot(const char *prefix)
{
    char relay_buf[48];

    snprintf(relay_buf, sizeof(relay_buf),
             "R1=%s,R2=%s",
             (Relay_GetState(RELAY1) == RELAY_ON) ? "ON" : "OFF",
             (Relay_GetState(RELAY2) == RELAY_ON) ? "ON" : "OFF");
    MQTT_DebugPrintLine(prefix, relay_buf);
}

static L610_MQTT_Status_t MQTT_EnqueueCommandRequest(const MQTT_CommandRequest_t *request)
{
    if (request == NULL || request->protocol_cmd[0] == '\0')
    {
        return L610_MQTT_INVALID_PARAM;
    }

    if (mqtt_cmd_queue_count >= MQTT_CMD_QUEUE_DEPTH)
    {
        MQTT_DebugPrint("[MQTT] CMD queue full\r\n");
        return L610_MQTT_ERROR;
    }

    memcpy(&mqtt_cmd_queue[mqtt_cmd_queue_tail], request, sizeof(*request));
    mqtt_cmd_queue_tail++;
    if (mqtt_cmd_queue_tail >= MQTT_CMD_QUEUE_DEPTH)
    {
        mqtt_cmd_queue_tail = 0U;
    }

    mqtt_cmd_queue_count++;
    mqtt_cmd_pending = 1U;
    return L610_MQTT_OK;
}

static L610_MQTT_Status_t MQTT_EnqueueLegacyCommand(const char *cmd)
{
    MQTT_CommandRequest_t request;

    if (cmd == NULL || cmd[0] == '\0')
    {
        return L610_MQTT_INVALID_PARAM;
    }

    MQTT_ClearCommandRequest(&request);
    MQTT_SafeCopy(request.protocol_cmd, sizeof(request.protocol_cmd), cmd);
    MQTT_SafeCopy(request.request_cmd, sizeof(request.request_cmd), cmd);
    MQTT_SafeCopy(request.device_id, sizeof(request.device_id), mqtt_config.client_id);
    snprintf(request.msg_id, sizeof(request.msg_id), "legacy-%lu", (unsigned long)HAL_GetTick());
    MQTT_SafeCopy(request.operator_name, sizeof(request.operator_name), "legacy");
    return MQTT_EnqueueCommandRequest(&request);
}

static L610_MQTT_Status_t MQTT_DequeueCommand(MQTT_CommandRequest_t *out_request)
{
    if (out_request == NULL)
    {
        return L610_MQTT_INVALID_PARAM;
    }

    if (mqtt_cmd_queue_count == 0U)
    {
        MQTT_ClearCommandRequest(out_request);
        mqtt_cmd_pending = 0U;
        return L610_MQTT_NO_DATA;
    }

    memcpy(out_request, &mqtt_cmd_queue[mqtt_cmd_queue_head], sizeof(*out_request));
    MQTT_ClearCommandRequest(&mqtt_cmd_queue[mqtt_cmd_queue_head]);
    mqtt_cmd_queue_head++;
    if (mqtt_cmd_queue_head >= MQTT_CMD_QUEUE_DEPTH)
    {
        mqtt_cmd_queue_head = 0U;
    }

    mqtt_cmd_queue_count--;
    mqtt_cmd_pending = (mqtt_cmd_queue_count != 0U) ? 1U : 0U;
    return L610_MQTT_OK;
}

static void MQTT_ClearTxQueue(void)
{
    memset(mqtt_tx_queue, 0, sizeof(mqtt_tx_queue));
    memset(mqtt_priority_queue, 0, sizeof(mqtt_priority_queue));
    mqtt_tx_head = 0U;
    mqtt_tx_tail = 0U;
    mqtt_tx_count = 0U;
    mqtt_priority_head = 0U;
    mqtt_priority_tail = 0U;
    mqtt_priority_count = 0U;
    mqtt_tx_inflight = 0U;
    mqtt_tx_inflight_priority = 0U;
    mqtt_tx_started_tick = 0U;
    mqtt_tx_ready_tick = 0U;
}

static void MQTT_DeferNextPublish(uint32_t delay_ms)
{
    mqtt_tx_ready_tick = HAL_GetTick() + delay_ms;
}

static void MQTT_ResetLinkToNetReady(void)
{
    uint8_t was_connected;

    was_connected = (mqtt_state >= MQTT_STATE_CONNECTED) ? 1U : 0U;
    MQTT_ClearTxQueue();
    MQTT_ResetStatusTracking();
    mqtt_state = MQTT_STATE_NET_READY;

    if (was_connected != 0U)
    {
        MQTT_EmitEvent(MQTT_EVENT_DISCONNECTED, mqtt_config.host);
    }
}

static void MQTT_BestEffortCloseSession(void)
{
    L610_MQTT_Status_t close_status;

    if (mqtt_state >= MQTT_STATE_SOCKET_OPENING)
    {
        MQTT_LogTx("AT+MQTTCLOSE=1");
        L610_SendCmd("AT+MQTTCLOSE=1\r\n");
        close_status = MQTT_WaitForKeywordOrOk("+MQTTCLOSE:", 3000);
        if (close_status != L610_MQTT_OK)
        {
            MQTT_DebugPrint("[MQTT] Session cleanup incomplete\r\n");
            L610_FlushRx();
        }
    }

    MQTT_ResetLinkToNetReady();
}

static uint8_t MQTT_IsStatusTopic(const char *topic)
{
    if (topic == NULL)
    {
        return 0U;
    }

    return (strcmp(topic, mqtt_config.topic_status) == 0) ? 1U : 0U;
}

static uint8_t MQTT_IsOnlineTopic(const char *topic)
{
    if (topic == NULL)
    {
        return 0U;
    }

    return (strcmp(topic, mqtt_config.topic_online) == 0) ? 1U : 0U;
}

static uint8_t MQTT_IsAckTopic(const char *topic)
{
    if (topic == NULL)
    {
        return 0U;
    }

    return (strcmp(topic, mqtt_config.topic_ack) == 0) ? 1U : 0U;
}

static int MQTT_FindQueuedTopicIndex(const char *topic)
{
    uint8_t i;
    uint8_t idx;
    uint8_t editable_offset;

    if (topic == NULL || mqtt_tx_count == 0U)
    {
        return -1;
    }

    idx = mqtt_tx_head;
    editable_offset = (mqtt_tx_inflight != 0U) ? 1U : 0U;

    for (i = 0U; i < mqtt_tx_count; i++)
    {
        if (i >= editable_offset && strcmp(mqtt_tx_queue[idx].topic, topic) == 0)
        {
            return (int)idx;
        }

        idx++;
        if (idx >= MQTT_TX_QUEUE_DEPTH)
        {
            idx = 0U;
        }
    }

    return -1;
}

static L610_MQTT_Status_t MQTT_QueuePublish(const char *topic, const char *payload, uint8_t qos, uint8_t retain)
{
    int existing_index;
    MQTT_TxQueueEntry_t *entry;

    if (topic == NULL || payload == NULL || topic[0] == '\0')
    {
        return L610_MQTT_INVALID_PARAM;
    }

    if (mqtt_state < MQTT_STATE_CONNECTED)
    {
        return L610_MQTT_NOT_READY;
    }

    if (MQTT_IsAckTopic(topic) != 0U)
    {
        return MQTT_QueuePriorityPublish(topic, payload, qos, retain);
    }

    existing_index = -1;
    if (MQTT_IsStatusTopic(topic) != 0U || MQTT_IsOnlineTopic(topic) != 0U)
    {
        existing_index = MQTT_FindQueuedTopicIndex(topic);
    }

    if (existing_index >= 0)
    {
        entry = &mqtt_tx_queue[existing_index];
        MQTT_SafeCopy(entry->payload, sizeof(entry->payload), payload);
        entry->qos = qos;
        entry->retain = retain;
        return L610_MQTT_OK;
    }

    if (mqtt_tx_count >= MQTT_TX_QUEUE_DEPTH)
    {
        if (MQTT_IsStatusTopic(topic) == 0U && MQTT_IsOnlineTopic(topic) == 0U)
        {
            MQTT_DebugPrint("[MQTT] TX queue full\r\n");
            return L610_MQTT_ERROR;
        }

        return L610_MQTT_NO_DATA;
    }

    entry = &mqtt_tx_queue[mqtt_tx_tail];
    memset(entry, 0, sizeof(*entry));
    MQTT_SafeCopy(entry->topic, sizeof(entry->topic), topic);
    MQTT_SafeCopy(entry->payload, sizeof(entry->payload), payload);
    entry->qos = qos;
    entry->retain = retain;

    mqtt_tx_tail++;
    if (mqtt_tx_tail >= MQTT_TX_QUEUE_DEPTH)
    {
        mqtt_tx_tail = 0U;
    }
    mqtt_tx_count++;
    return L610_MQTT_OK;
}

static L610_MQTT_Status_t MQTT_QueuePriorityPublish(const char *topic, const char *payload, uint8_t qos, uint8_t retain)
{
    MQTT_TxQueueEntry_t *entry;

    if (topic == NULL || payload == NULL || topic[0] == '\0')
    {
        return L610_MQTT_INVALID_PARAM;
    }

    if (mqtt_state < MQTT_STATE_CONNECTED)
    {
        return L610_MQTT_NOT_READY;
    }

    if (mqtt_priority_count >= MQTT_PRIORITY_QUEUE_DEPTH)
    {
        MQTT_DebugPrint("[MQTT] Priority queue full\r\n");
        return L610_MQTT_ERROR;
    }

    entry = &mqtt_priority_queue[mqtt_priority_tail];
    memset(entry, 0, sizeof(*entry));
    MQTT_SafeCopy(entry->topic, sizeof(entry->topic), topic);
    MQTT_SafeCopy(entry->payload, sizeof(entry->payload), payload);
    entry->qos = qos;
    entry->retain = retain;

    mqtt_priority_tail++;
    if (mqtt_priority_tail >= MQTT_PRIORITY_QUEUE_DEPTH)
    {
        mqtt_priority_tail = 0U;
    }
    mqtt_priority_count++;
    return L610_MQTT_OK;
}

static void MQTT_FinishQueuedPublish(L610_MQTT_Status_t result)
{
    MQTT_TxQueueEntry_t *entry;

    if (mqtt_priority_count == 0U && mqtt_tx_count == 0U)
    {
        mqtt_tx_inflight = 0U;
        mqtt_tx_inflight_priority = 0U;
        mqtt_tx_started_tick = 0U;
        return;
    }

    if (mqtt_tx_inflight_priority != 0U)
    {
        entry = &mqtt_priority_queue[mqtt_priority_head];
    }
    else
    {
        entry = &mqtt_tx_queue[mqtt_tx_head];
    }

    if (result == L610_MQTT_OK)
    {
        if (MQTT_IsStatusTopic(entry->topic) != 0U)
        {
            mqtt_status_last_publish_tick = HAL_GetTick();
        }
        MQTT_DebugPrintLine("[MQTT] Publish OK: ", entry->topic);
        MQTT_EmitEvent(MQTT_EVENT_PUBLISH_OK, entry->topic);
    }
    else
    {
        MQTT_DebugPrintLine("[MQTT] Publish failed: ", entry->topic);
        MQTT_EmitEvent(MQTT_EVENT_PUBLISH_ERROR, entry->topic);
        if (MQTT_IsStatusTopic(entry->topic) != 0U)
        {
            MQTT_RequestStatusPublishInternal();
        }
    }

    memset(entry, 0, sizeof(*entry));
    if (mqtt_tx_inflight_priority != 0U)
    {
        mqtt_priority_head++;
        if (mqtt_priority_head >= MQTT_PRIORITY_QUEUE_DEPTH)
        {
            mqtt_priority_head = 0U;
        }
        mqtt_priority_count--;
    }
    else
    {
        mqtt_tx_head++;
        if (mqtt_tx_head >= MQTT_TX_QUEUE_DEPTH)
        {
            mqtt_tx_head = 0U;
        }
        mqtt_tx_count--;
    }
    mqtt_tx_inflight = 0U;
    mqtt_tx_inflight_priority = 0U;
    mqtt_tx_started_tick = 0U;
}

static void MQTT_StartNextQueuedPublish(void)
{
    MQTT_TxQueueEntry_t *entry;

    if (mqtt_tx_inflight != 0U || (mqtt_priority_count == 0U && mqtt_tx_count == 0U))
    {
        return;
    }

    if (mqtt_state < MQTT_STATE_CONNECTED || mqtt_async_len != 0U)
    {
        return;
    }

    if (mqtt_priority_count == 0U &&
        mqtt_tx_ready_tick != 0U &&
        (int32_t)(HAL_GetTick() - mqtt_tx_ready_tick) < 0)
    {
        return;
    }

    if (mqtt_priority_count != 0U)
    {
        entry = &mqtt_priority_queue[mqtt_priority_head];
        mqtt_tx_inflight_priority = 1U;
    }
    else
    {
        entry = &mqtt_tx_queue[mqtt_tx_head];
        mqtt_tx_inflight_priority = 0U;
    }
    MQTT_EscapeForAT(entry->topic, mqtt_topic_tx_buf, sizeof(mqtt_topic_tx_buf));
    MQTT_EscapeForAT(entry->payload, mqtt_payload_tx_buf, sizeof(mqtt_payload_tx_buf));
    snprintf(mqtt_cmd_tx_buf, sizeof(mqtt_cmd_tx_buf),
             "AT+MQTTPUB=1,\"%s\",%u,%u,\"%s\"\r\n",
             mqtt_topic_tx_buf,
             (unsigned int)entry->qos,
             (unsigned int)entry->retain,
             mqtt_payload_tx_buf);

    if (MQTT_DEBUG_COMMAND_TRACE != 0U)
    {
        MQTT_DebugPrint("[MQTT] Publish\r\n");
        MQTT_DebugPrintLine("  Topic: ", entry->topic);
    }
    if (MQTT_DEBUG_VERBOSE_PAYLOAD != 0U)
    {
        MQTT_DebugPrintLine("  Payload: ", entry->payload);
    }

    MQTT_LogTx(mqtt_cmd_tx_buf);
    L610_SendCmd(mqtt_cmd_tx_buf);
    mqtt_tx_inflight = 1U;
    mqtt_tx_started_tick = HAL_GetTick();
}

static void MQTT_HandlePublishTimeout(void)
{
    MQTT_TxQueueEntry_t *entry;
    uint32_t elapsed_ms;

    if (mqtt_tx_inflight == 0U)
    {
        return;
    }

    if (mqtt_tx_inflight_priority != 0U)
    {
        entry = &mqtt_priority_queue[mqtt_priority_head];
    }
    else
    {
        entry = &mqtt_tx_queue[mqtt_tx_head];
    }
    elapsed_ms = HAL_GetTick() - mqtt_tx_started_tick;

    if (entry->qos == 0U)
    {
        if (elapsed_ms < MQTT_PUBLISH_QOS0_SETTLE_MS)
        {
            return;
        }

        MQTT_DebugPrintLine("[MQTT] Publish implicit OK: ", entry->topic);
        MQTT_FinishQueuedPublish(L610_MQTT_OK);
        return;
    }

    if (elapsed_ms < MQTT_PUBLISH_ACK_TIMEOUT_MS)
    {
        return;
    }

    MQTT_LogFailureContext("[MQTT] Publish wait timeout");
    MQTT_FinishQueuedPublish(L610_MQTT_TIMEOUT);
    MQTT_BestEffortCloseSession();
}

static void MQTT_RunRealtimeStatusPublisher(void)
{
    RelayState_t relay1_state;
    RelayState_t relay2_state;
    L610_MQTT_Status_t status;
    uint8_t fast_window_active;

    if (L610_MQTT_IsConnected() == 0)
    {
        return;
    }

    fast_window_active = ((int32_t)(HAL_GetTick() - mqtt_command_fast_until) < 0) ? 1U : 0U;

    relay1_state = Relay_GetState(RELAY1);
    relay2_state = Relay_GetState(RELAY2);

    if (mqtt_status_snapshot_valid == 0U ||
        relay1_state != mqtt_status_last_relay1 ||
        relay2_state != mqtt_status_last_relay2)
    {
        mqtt_status_last_relay1 = relay1_state;
        mqtt_status_last_relay2 = relay2_state;
        mqtt_status_snapshot_valid = 1U;
        if (fast_window_active != 0U)
        {
            mqtt_status_priority_requested = 1U;
        }
        else
        {
            MQTT_RequestStatusPublishInternal();
        }
    }

    if (mqtt_status_priority_requested != 0U)
    {
        status = L610_MQTT_BuildStatusJson(mqtt_json_buf, sizeof(mqtt_json_buf));
        if (status != L610_MQTT_OK)
        {
            return;
        }

        status = MQTT_QueuePriorityPublish(mqtt_config.topic_status, mqtt_json_buf, mqtt_config.default_qos, 1U);
        if (status == L610_MQTT_OK)
        {
            mqtt_status_priority_requested = 0U;
        }
        return;
    }

    if (mqtt_status_publish_requested == 0U)
    {
        return;
    }

    if (fast_window_active != 0U && (mqtt_priority_count != 0U || mqtt_tx_inflight_priority != 0U))
    {
        return;
    }

    status = L610_MQTT_BuildStatusJson(mqtt_json_buf, sizeof(mqtt_json_buf));
    if (status != L610_MQTT_OK)
    {
        return;
    }

    status = MQTT_QueuePublish(mqtt_config.topic_status, mqtt_json_buf, mqtt_config.default_qos, 1U);
    if (status == L610_MQTT_OK)
    {
        mqtt_status_publish_requested = 0U;
    }
}

static void MQTT_RunOnlinePublisher(void)
{
    if (((int32_t)(HAL_GetTick() - mqtt_command_fast_until) < 0) &&
        (mqtt_priority_count != 0U || mqtt_tx_inflight_priority != 0U))
    {
        return;
    }

    if (L610_MQTT_IsConnected() == 0 || mqtt_online_publish_requested == 0U)
    {
        return;
    }

    if (L610_MQTT_PublishOnlineState(mqtt_online_state) == L610_MQTT_OK)
    {
        mqtt_online_publish_requested = 0U;
    }
}

static void MQTT_RunLinkMaintenance(void)
{
    L610_MQTT_Status_t status;

    if (mqtt_state < MQTT_STATE_CONNECTED)
    {
        return;
    }

    if (mqtt_async_len != 0U || mqtt_tx_inflight != 0U || mqtt_priority_count != 0U)
    {
        return;
    }

    if (mqtt_subscribe_refresh_pending != 0U)
    {
        if ((int32_t)(HAL_GetTick() - mqtt_subscribe_refresh_tick) < 0)
        {
            return;
        }

        mqtt_subscribe_refresh_pending = 0U;
        mqtt_subscribe_refresh_done = 1U;
        MQTT_DebugPrint("[MQTT] Subscribe refresh...\r\n");
        status = L610_MQTT_SubscribeCmd();
        MQTT_DebugPrintLine("[MQTT] Subscribe refresh result: ", L610_MQTT_GetStatusString(status));
        return;
    }

    if (mqtt_status_publish_requested != 0U)
    {
        return;
    }

    if (mqtt_status_last_publish_tick == 0U)
    {
        return;
    }

#if MQTT_FEATURE_KEEPALIVE_STATUS
    if ((HAL_GetTick() - mqtt_status_last_publish_tick) >= MQTT_STATUS_KEEPALIVE_MS)
    {
        MQTT_DebugPrint("[MQTT] Keepalive status request\r\n");
        MQTT_RequestStatusPublishInternal();
    }
#endif
}

static void MQTT_CopySegmentTrim(const char *start, const char *end, char *dst, uint16_t dst_size)
{
    uint16_t len;

    if (dst == NULL || dst_size == 0)
    {
        return;
    }

    dst[0] = '\0';
    if (start == NULL || end == NULL || end < start)
    {
        return;
    }

    while (start < end && (*start == ' ' || *start == '\t' || *start == ','))
    {
        start++;
    }

    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
    {
        end--;
    }

    if (start < end && *start == '"' && end > start + 1 && end[-1] == '"')
    {
        start++;
        end--;
    }

    len = (uint16_t)(end - start);
    if (len >= dst_size)
    {
        len = dst_size - 1;
    }

    if (len > 0)
    {
        memcpy(dst, start, len);
    }
    dst[len] = '\0';
}

static const char *MQTT_FindIncomingPrefix(const char *line)
{
    const char *prefix;

    if (line == NULL)
    {
        return NULL;
    }

    prefix = strstr(line, "+MQTTMSG:");
    if (prefix != NULL)
    {
        return prefix;
    }

    return strstr(line, "TMSG:");
}

static void MQTT_Trim(char *str)
{
    char *start;
    uint16_t len;

    if (str == NULL)
    {
        return;
    }

    start = str;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
    {
        start++;
    }

    if (start != str)
    {
        memmove(str, start, strlen(start) + 1);
    }

    MQTT_TrimRight(str);

    len = (uint16_t)strlen(str);
    if (len >= 2 && str[0] == '"' && str[len - 1] == '"')
    {
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
    }
}

static int MQTT_ParseJsonStringField(const char *json, const char *field_name, char *out, uint16_t out_size)
{
    char pattern[24];
    const char *p;
    const char *match;
    uint16_t used;

    if (json == NULL || field_name == NULL || out == NULL || out_size == 0)
    {
        return 0;
    }

    if (snprintf(pattern, sizeof(pattern), "\"%s\"", field_name) >= (int)sizeof(pattern))
    {
        return 0;
    }

    match = strstr(json, pattern);
    if (match == NULL)
    {
        return 0;
    }

    p = match + strlen(pattern);
    while (*p == ' ' || *p == '\t')
    {
        p++;
    }

    if (*p != ':')
    {
        return 0;
    }

    p++;
    while (*p == ' ' || *p == '\t')
    {
        p++;
    }

    if (*p != '"')
    {
        return 0;
    }

    p++;
    used = 0;
    while (*p != '\0' && used < out_size - 1)
    {
        if (*p == '\\')
        {
            p++;
            if (*p == '\0')
            {
                break;
            }

            if (*p == 'n')
            {
                out[used++] = '\n';
            }
            else if (*p == 'r')
            {
                out[used++] = '\r';
            }
            else if (*p == 't')
            {
                out[used++] = '\t';
            }
            else
            {
                out[used++] = *p;
            }

            p++;
            continue;
        }

        if (*p == '"')
        {
            out[used] = '\0';
            MQTT_Trim(out);
            return 1;
        }

        out[used++] = *p++;
    }

    out[used] = '\0';
    return 0;
}

static int MQTT_ParseJsonIntField(const char *json, const char *field_name, int *out_value)
{
    char pattern[24];
    const char *p;
    const char *match;
    char *endptr;
    long parsed_value;

    if (json == NULL || field_name == NULL || out_value == NULL)
    {
        return 0;
    }

    if (snprintf(pattern, sizeof(pattern), "\"%s\"", field_name) >= (int)sizeof(pattern))
    {
        return 0;
    }

    match = strstr(json, pattern);
    if (match == NULL)
    {
        return 0;
    }

    p = match + strlen(pattern);
    while (*p == ' ' || *p == '\t')
    {
        p++;
    }

    if (*p != ':')
    {
        return 0;
    }

    p++;
    while (*p == ' ' || *p == '\t')
    {
        p++;
    }

    parsed_value = strtol(p, &endptr, 10);
    if (endptr == p)
    {
        return 0;
    }

    *out_value = (int)parsed_value;
    return 1;
}

static int MQTT_FindKnownCommandToken(const char *text, char *cmd_out, uint16_t cmd_size)
{
    static const char *known_cmds[] =
    {
        "ALL_OFF",
        "ALL_ON",
        "R1_OFF",
        "R1_ON",
        "R2_OFF",
        "R2_ON",
        "STATUS"
    };
    uint32_t i;
    const char *match;

    if (text == NULL || cmd_out == NULL || cmd_size == 0U)
    {
        return 0;
    }

    cmd_out[0] = '\0';
    for (i = 0; i < (sizeof(known_cmds) / sizeof(known_cmds[0])); i++)
    {
        match = strstr(text, known_cmds[i]);
        if (match != NULL)
        {
            MQTT_SafeCopy(cmd_out, cmd_size, known_cmds[i]);
            return 1;
        }
    }

    return 0;
}

static int MQTT_MapLegacyCommand(const char *legacy_cmd, MQTT_CommandRequest_t *request)
{
    if (legacy_cmd == NULL || request == NULL)
    {
        return 0;
    }

    MQTT_SafeCopy(request->request_cmd, sizeof(request->request_cmd), legacy_cmd);
    if (strcmp(legacy_cmd, "R1_ON") == 0)
    {
        MQTT_SafeCopy(request->protocol_cmd, sizeof(request->protocol_cmd), "R1_ON");
        request->has_value = 1U;
        request->value = 1;
        return 1;
    }
    if (strcmp(legacy_cmd, "R1_OFF") == 0)
    {
        MQTT_SafeCopy(request->protocol_cmd, sizeof(request->protocol_cmd), "R1_OFF");
        request->has_value = 1U;
        request->value = 0;
        return 1;
    }
    if (strcmp(legacy_cmd, "R2_ON") == 0)
    {
        MQTT_SafeCopy(request->protocol_cmd, sizeof(request->protocol_cmd), "R2_ON");
        request->has_value = 1U;
        request->value = 1;
        return 1;
    }
    if (strcmp(legacy_cmd, "R2_OFF") == 0)
    {
        MQTT_SafeCopy(request->protocol_cmd, sizeof(request->protocol_cmd), "R2_OFF");
        request->has_value = 1U;
        request->value = 0;
        return 1;
    }
    if (strcmp(legacy_cmd, "ALL_ON") == 0)
    {
        MQTT_SafeCopy(request->protocol_cmd, sizeof(request->protocol_cmd), "ALL_ON");
        request->has_value = 1U;
        request->value = 1;
        return 1;
    }
    if (strcmp(legacy_cmd, "ALL_OFF") == 0)
    {
        MQTT_SafeCopy(request->protocol_cmd, sizeof(request->protocol_cmd), "ALL_OFF");
        request->has_value = 1U;
        request->value = 0;
        return 1;
    }
    if (strcmp(legacy_cmd, "STATUS") == 0)
    {
        MQTT_SafeCopy(request->protocol_cmd, sizeof(request->protocol_cmd), "STATUS");
        request->has_value = 0U;
        request->value = 0;
        return 1;
    }

    return 0;
}

static L610_MQTT_Status_t MQTT_ParseIncomingCommandPayload(const char *payload, MQTT_CommandRequest_t *request)
{
    char cmd_name[MQTT_CMD_NAME_BUF_SIZE];
    char legacy_cmd[MQTT_CMD_BUF_SIZE];
    int value;

    if (payload == NULL || request == NULL)
    {
        return L610_MQTT_INVALID_PARAM;
    }

    MQTT_ClearCommandRequest(request);
    MQTT_SafeCopy(request->device_id, sizeof(request->device_id), mqtt_config.client_id);
    MQTT_SafeCopy(request->operator_name, sizeof(request->operator_name), "mqtt");

    if (MQTT_ParseJsonStringField(payload, "msg_id", request->msg_id, sizeof(request->msg_id)) == 0)
    {
        snprintf(request->msg_id, sizeof(request->msg_id), "cmd-%lu", (unsigned long)HAL_GetTick());
    }

    MQTT_ParseJsonStringField(payload, "device_id", request->device_id, sizeof(request->device_id));
    MQTT_ParseJsonStringField(payload, "operator", request->operator_name, sizeof(request->operator_name));

    if (request->device_id[0] == '\0')
    {
        MQTT_SafeCopy(request->device_id, sizeof(request->device_id), mqtt_config.client_id);
    }

    if (strcmp(request->device_id, mqtt_config.client_id) != 0)
    {
        return L610_MQTT_INVALID_PARAM;
    }

    cmd_name[0] = '\0';
    if (MQTT_ParseJsonStringField(payload, "cmd", cmd_name, sizeof(cmd_name)) != 0 ||
        MQTT_ParseJsonStringField(payload, "command", cmd_name, sizeof(cmd_name)) != 0)
    {
        if (strcmp(cmd_name, MQTT_CMD_SET_R1) == 0)
        {
            if (MQTT_ParseJsonIntField(payload, "value", &value) == 0 || (value != 0 && value != 1))
            {
                return L610_MQTT_INVALID_PARAM;
            }
            MQTT_SafeCopy(request->request_cmd, sizeof(request->request_cmd), MQTT_CMD_SET_R1);
            MQTT_SafeCopy(request->protocol_cmd, sizeof(request->protocol_cmd), (value != 0) ? "R1_ON" : "R1_OFF");
            request->has_value = 1U;
            request->value = value;
            return L610_MQTT_OK;
        }

        if (strcmp(cmd_name, MQTT_CMD_SET_R2) == 0)
        {
            if (MQTT_ParseJsonIntField(payload, "value", &value) == 0 || (value != 0 && value != 1))
            {
                return L610_MQTT_INVALID_PARAM;
            }
            MQTT_SafeCopy(request->request_cmd, sizeof(request->request_cmd), MQTT_CMD_SET_R2);
            MQTT_SafeCopy(request->protocol_cmd, sizeof(request->protocol_cmd), (value != 0) ? "R2_ON" : "R2_OFF");
            request->has_value = 1U;
            request->value = value;
            return L610_MQTT_OK;
        }

        if (strcmp(cmd_name, MQTT_CMD_SET_ALL) == 0)
        {
            if (MQTT_ParseJsonIntField(payload, "value", &value) == 0 || (value != 0 && value != 1))
            {
                return L610_MQTT_INVALID_PARAM;
            }
            MQTT_SafeCopy(request->request_cmd, sizeof(request->request_cmd), MQTT_CMD_SET_ALL);
            MQTT_SafeCopy(request->protocol_cmd, sizeof(request->protocol_cmd), (value != 0) ? "ALL_ON" : "ALL_OFF");
            request->has_value = 1U;
            request->value = value;
            return L610_MQTT_OK;
        }

        if (strcmp(cmd_name, MQTT_CMD_QUERY_STATUS) == 0)
        {
            MQTT_SafeCopy(request->request_cmd, sizeof(request->request_cmd), MQTT_CMD_QUERY_STATUS);
            MQTT_SafeCopy(request->protocol_cmd, sizeof(request->protocol_cmd), "STATUS");
            request->has_value = 0U;
            request->value = 0;
            return L610_MQTT_OK;
        }

        if (strcmp(cmd_name, MQTT_CMD_PAD_OPEN) == 0)
        {
            MQTT_SafeCopy(request->request_cmd, sizeof(request->request_cmd), MQTT_CMD_PAD_OPEN);
            MQTT_SafeCopy(request->protocol_cmd, sizeof(request->protocol_cmd), "PAD_OPEN");
            request->has_value = 0U;
            request->value = 0;
            return L610_MQTT_OK;
        }

        if (strcmp(cmd_name, MQTT_CMD_PAD_CLOSE) == 0)
        {
            MQTT_SafeCopy(request->request_cmd, sizeof(request->request_cmd), MQTT_CMD_PAD_CLOSE);
            MQTT_SafeCopy(request->protocol_cmd, sizeof(request->protocol_cmd), "PAD_CLOSE");
            request->has_value = 0U;
            request->value = 0;
            return L610_MQTT_OK;
        }

        if (strcmp(cmd_name, MQTT_CMD_PAD_STOP) == 0)
        {
            MQTT_SafeCopy(request->request_cmd, sizeof(request->request_cmd), MQTT_CMD_PAD_STOP);
            MQTT_SafeCopy(request->protocol_cmd, sizeof(request->protocol_cmd), "PAD_STOP");
            request->has_value = 0U;
            request->value = 0;
            return L610_MQTT_OK;
        }

        if (strcmp(cmd_name, MQTT_CMD_QUERY_PAD_STATUS) == 0)
        {
            MQTT_SafeCopy(request->request_cmd, sizeof(request->request_cmd), MQTT_CMD_QUERY_PAD_STATUS);
            MQTT_SafeCopy(request->protocol_cmd, sizeof(request->protocol_cmd), "QUERY_PAD_STATUS");
            request->has_value = 0U;
            request->value = 0;
            return L610_MQTT_OK;
        }

        if (MQTT_MapLegacyCommand(cmd_name, request) != 0)
        {
            return L610_MQTT_OK;
        }

        return L610_MQTT_PARSE_ERROR;
    }

    legacy_cmd[0] = '\0';
    if (MQTT_ParseJsonStringField(payload, "message", legacy_cmd, sizeof(legacy_cmd)) != 0 ||
        MQTT_FindKnownCommandToken(payload, legacy_cmd, sizeof(legacy_cmd)) != 0)
    {
        if (MQTT_MapLegacyCommand(legacy_cmd, request) != 0)
        {
            return L610_MQTT_OK;
        }
    }

    return L610_MQTT_PARSE_ERROR;
}

static void MQTT_ClearAsyncBuffer(void)
{
    memset(mqtt_async_buf, 0, sizeof(mqtt_async_buf));
    mqtt_async_len = 0;
    mqtt_async_last_tick = 0;
}

static uint16_t MQTT_TakeAsyncFragment(char *out_buf, uint16_t buf_size)
{
    uint16_t copy_len;

    if (out_buf == NULL || buf_size == 0 || mqtt_async_len == 0)
    {
        return 0;
    }

    copy_len = mqtt_async_len;
    if (copy_len >= buf_size)
    {
        copy_len = buf_size - 1;
    }

    if (copy_len > 0)
    {
        memcpy(out_buf, mqtt_async_buf, copy_len);
        out_buf[copy_len] = '\0';
    }

    MQTT_ClearAsyncBuffer();
    return copy_len;
}

static uint8_t MQTT_LineLooksProcessable(const char *line)
{
    if (line == NULL || line[0] == '\0')
    {
        return 0;
    }

    if (strstr(line, "+MQTTMSG:") != NULL ||
        strstr(line, "TMSG:") != NULL ||
        strstr(line, "+MQTTOPEN:") != NULL ||
        strstr(line, "+MQTTCONN:") != NULL ||
        strstr(line, "+MQTTSUB:") != NULL ||
        strstr(line, "+MQTTPUB:") != NULL ||
        strstr(line, "+MQTTCLOSE:") != NULL ||
        strstr(line, "+MIPCALL:") != NULL ||
        strstr(line, "+CME ERROR") != NULL ||
        strstr(line, "ERROR") != NULL ||
        strcmp(line, "OK") == 0)
    {
        return 1;
    }

    return 0;
}

static void MQTT_ProcessAsyncLineBuffer(void)
{
    MQTT_TrimRight(mqtt_async_buf);
    if (mqtt_async_buf[0] != '\0')
    {
        if (MQTT_DEBUG_VERBOSE_IO != 0U)
        {
            MQTT_DebugPrintLine("[MQTT][RX] ", mqtt_async_buf);
        }
        if (MQTT_LineLooksProcessable(mqtt_async_buf) != 0U)
        {
            if (MQTT_FindIncomingPrefix(mqtt_async_buf) != NULL)
            {
                MQTT_DebugPrintLine("[MQTT] RX MQTT URC: ", mqtt_async_buf);
            }

            L610_MQTT_ProcessLine(mqtt_async_buf);
        }
    }

    MQTT_ClearAsyncBuffer();
}

static L610_MQTT_Status_t MQTT_ReadLineResponse(char *out_buf, uint16_t buf_size, uint32_t timeout_ms)
{
    uint32_t start;
    uint16_t len;
    uint8_t ch;

    if (out_buf == NULL || buf_size == 0)
    {
        return L610_MQTT_INVALID_PARAM;
    }

    start = HAL_GetTick();
    len = MQTT_TakeAsyncFragment(out_buf, buf_size);
    ch = 0;
    if (len == 0)
    {
        memset(out_buf, 0, buf_size);
    }

    while (HAL_GetTick() - start < timeout_ms)
    {
        if (HAL_UART_Receive(&huart1, &ch, 1, MQTT_RX_POLL_TIMEOUT_MS) == HAL_OK)
        {
            if (len < buf_size - 1)
            {
                out_buf[len++] = (char)ch;
                out_buf[len] = '\0';
            }

            if (ch == '\n')
            {
                MQTT_TrimRight(out_buf);
                return L610_MQTT_OK;
            }
        }
    }

    MQTT_TrimRight(out_buf);
    return L610_MQTT_TIMEOUT;
}

static L610_MQTT_Status_t MQTT_ReadPromptResponse(char *out_buf, uint16_t buf_size, uint32_t timeout_ms)
{
    uint32_t start;
    uint16_t len;
    uint8_t ch;

    if (out_buf == NULL || buf_size == 0)
    {
        return L610_MQTT_INVALID_PARAM;
    }

    start = HAL_GetTick();
    len = MQTT_TakeAsyncFragment(out_buf, buf_size);
    ch = 0;
    if (len == 0)
    {
        memset(out_buf, 0, buf_size);
    }

    while (HAL_GetTick() - start < timeout_ms)
    {
        if (HAL_UART_Receive(&huart1, &ch, 1, MQTT_RX_POLL_TIMEOUT_MS) == HAL_OK)
        {
            if (len < buf_size - 1)
            {
                out_buf[len++] = (char)ch;
                out_buf[len] = '\0';
            }

            if (ch == '>')
            {
                MQTT_TrimRight(out_buf);
                return L610_MQTT_OK;
            }
        }
    }

    MQTT_TrimRight(out_buf);
    return L610_MQTT_TIMEOUT;
}

static int MQTT_ParseTrailingResultCode(const char *line, int *result_code)
{
    const char *p;
    char *endptr;
    long value;

    if (line == NULL || result_code == NULL)
    {
        return 0;
    }

    p = strrchr(line, ',');
    if (p == NULL)
    {
        return 0;
    }

    p++;
    while (*p == ' ' || *p == '\t')
    {
        p++;
    }

    value = strtol(p, &endptr, 10);
    if (endptr == p)
    {
        return 0;
    }

    *result_code = (int)value;
    return 1;
}

static L610_MQTT_Status_t MQTT_CheckKeywordResult(const char *keyword, const char *line)
{
    int result_code;
    uint8_t require_result_code;

    if (keyword == NULL || line == NULL || strstr(line, keyword) == NULL)
    {
        return L610_MQTT_NO_DATA;
    }

    if (strcmp(keyword, "+MIPCALL:") == 0)
    {
        if (MQTT_MIPCALLHasValidIP(line))
        {
            mqtt_last_result_code = 0;
            return L610_MQTT_OK;
        }

        if (strstr(line, "0.0.0.0") != NULL)
        {
            mqtt_last_result_code = -1;
            return L610_MQTT_NO_DATA;
        }

        if (MQTT_ParseTrailingResultCode(line, &result_code) != 0)
        {
            mqtt_last_result_code = result_code;
            return (result_code == 0) ? L610_MQTT_OK : L610_MQTT_ERROR;
        }

        mqtt_last_result_code = -1;
        return L610_MQTT_ERROR;
    }

    if (MQTT_ParseTrailingResultCode(line, &result_code) != 0)
    {
        mqtt_last_result_code = result_code;
        if (strcmp(keyword, "+MQTTOPEN:") == 0 ||
            strcmp(keyword, "+MQTTSUB:") == 0 ||
            strcmp(keyword, "+MQTTPUB:") == 0 ||
            strcmp(keyword, "+MQTTCONN:") == 0)
        {
            return (result_code == 0 || result_code == 1) ? L610_MQTT_OK : L610_MQTT_ERROR;
        }

        return (result_code == 0) ? L610_MQTT_OK : L610_MQTT_ERROR;
    }

    require_result_code = (strcmp(keyword, "+MQTTOPEN:") == 0 ||
                           strcmp(keyword, "+MQTTSUB:") == 0 ||
                           strcmp(keyword, "+MQTTPUB:") == 0 ||
                           strcmp(keyword, "+MQTTCONN:") == 0) ? 1U : 0U;
    if (require_result_code != 0)
    {
        return L610_MQTT_NO_DATA;
    }

    mqtt_last_result_code = 0;
    return L610_MQTT_OK;
}

static L610_MQTT_Status_t MQTT_WaitForKeyword(const char *keyword, uint32_t timeout_ms)
{
    uint32_t start;
    L610_MQTT_Status_t read_status;
    L610_MQTT_Status_t match_status;
    uint32_t remaining_ms;

    if (keyword == NULL)
    {
        return L610_MQTT_INVALID_PARAM;
    }

    start = HAL_GetTick();
    while (HAL_GetTick() - start < timeout_ms)
    {
        remaining_ms = timeout_ms - (HAL_GetTick() - start);
        read_status = MQTT_ReadLineResponse(mqtt_line_buf, sizeof(mqtt_line_buf), remaining_ms);
        if (read_status == L610_MQTT_TIMEOUT)
        {
            continue;
        }

        if (read_status != L610_MQTT_OK)
        {
            return read_status;
        }

        if (mqtt_line_buf[0] == '\0')
        {
            continue;
        }

        if (MQTT_DEBUG_VERBOSE_IO != 0U)
        {
            MQTT_DebugPrintLine("[MQTT][RX] ", mqtt_line_buf);
        }
        L610_MQTT_ProcessLine(mqtt_line_buf);

        if (strstr(mqtt_line_buf, "ERROR") != NULL || strstr(mqtt_line_buf, "+CME ERROR") != NULL)
        {
            mqtt_last_result_code = -1;
            return L610_MQTT_ERROR;
        }

        match_status = MQTT_CheckKeywordResult(keyword, mqtt_line_buf);
        if (match_status == L610_MQTT_OK || match_status == L610_MQTT_ERROR)
        {
            return match_status;
        }
    }

    return L610_MQTT_TIMEOUT;
}

static L610_MQTT_Status_t MQTT_WaitForKeywordOrOk(const char *keyword, uint32_t timeout_ms)
{
    uint32_t start;
    uint32_t remaining_ms;
    L610_MQTT_Status_t read_status;
    L610_MQTT_Status_t match_status;

    start = HAL_GetTick();
    while (HAL_GetTick() - start < timeout_ms)
    {
        remaining_ms = timeout_ms - (HAL_GetTick() - start);
        read_status = MQTT_ReadLineResponse(mqtt_line_buf, sizeof(mqtt_line_buf), remaining_ms);
        if (read_status == L610_MQTT_TIMEOUT)
        {
            continue;
        }

        if (read_status != L610_MQTT_OK)
        {
            return read_status;
        }

        if (mqtt_line_buf[0] == '\0')
        {
            continue;
        }

        if (MQTT_DEBUG_VERBOSE_IO != 0U)
        {
            MQTT_DebugPrintLine("[MQTT][RX] ", mqtt_line_buf);
        }
        L610_MQTT_ProcessLine(mqtt_line_buf);

        if (strstr(mqtt_line_buf, "ERROR") != NULL || strstr(mqtt_line_buf, "+CME ERROR") != NULL)
        {
            mqtt_last_result_code = -1;
            return L610_MQTT_ERROR;
        }

        if (keyword != NULL && strstr(mqtt_line_buf, keyword) != NULL)
        {
            match_status = MQTT_CheckKeywordResult(keyword, mqtt_line_buf);
            if (match_status == L610_MQTT_OK || match_status == L610_MQTT_ERROR)
            {
                return match_status;
            }
        }

        if (strcmp(mqtt_line_buf, "OK") == 0)
        {
            mqtt_last_result_code = 0;
            return L610_MQTT_OK;
        }
    }

    return L610_MQTT_TIMEOUT;
}

static L610_MQTT_Status_t MQTT_CheckIPAlreadyReady(void)
{
    char *buf;

    if (L610_SendAndWait("AT+MIPCALL?\r\n", 1500) != L610_OK)
    {
        return L610_MQTT_ERROR;
    }

    buf = L610_GetBuffer();
    MQTT_DebugPrint("[MQTT] MIPCALL? raw:\r\n");
    MQTT_DebugPrint(buf);
    MQTT_DebugPrint("\r\n");

    if (strstr(buf, "+MIPCALL: 1") != NULL && MQTT_HasNonZeroIP(buf))
    {
        return L610_MQTT_OK;
    }

    if (strchr(buf, '"') != NULL && MQTT_HasNonZeroIP(buf))
    {
        return L610_MQTT_OK;
    }

    return L610_MQTT_NO_DATA;
}

static L610_MQTT_Status_t MQTT_ResetIPSession(void)
{
    L610_MQTT_Status_t status;

    MQTT_DebugPrint("[MQTT] Reset stale MIPCALL...\r\n");
    MQTT_LogTx("AT+MIPCALL=0");
    L610_SendCmd("AT+MIPCALL=0\r\n");
    status = MQTT_WaitForKeywordOrOk("+MIPCALL:", 15000);
    if (status != L610_MQTT_OK)
    {
        MQTT_DebugPrint("[MQTT] Reset stale MIPCALL incomplete\r\n");
    }

    return status;
}

static void MQTT_StoreMessage(int client_id, int qos, const char *topic, const char *payload, const char *raw_line)
{
    mqtt_last_message.client_id = client_id;
    mqtt_last_message.qos = qos;
    mqtt_last_message.tick = HAL_GetTick();
    MQTT_SafeCopy(mqtt_last_message.topic, sizeof(mqtt_last_message.topic), topic);
    MQTT_SafeCopy(mqtt_last_message.payload, sizeof(mqtt_last_message.payload), payload);
    MQTT_SafeCopy(mqtt_last_message.raw_line, sizeof(mqtt_last_message.raw_line), raw_line);
    mqtt_message_pending = 1;

    if (mqtt_message_callback != NULL)
    {
        mqtt_message_callback(&mqtt_last_message);
    }

    MQTT_EmitEvent(MQTT_EVENT_MESSAGE, mqtt_last_message.topic);
}

static L610_MQTT_Status_t MQTT_ProcessPendingCommand(void)
{
    L610_MQTT_Status_t status;

    if (MQTT_DequeueCommand(&mqtt_cmd_current) != L610_MQTT_OK)
    {
        return L610_MQTT_NO_DATA;
    }

    MQTT_SafeCopy(mqtt_cmd_buf, sizeof(mqtt_cmd_buf), mqtt_cmd_current.protocol_cmd);
    status = L610_MQTT_HandleCommand(mqtt_cmd_buf);
    if (status == L610_MQTT_OK)
    {
        MQTT_RememberRecentMsgId(mqtt_cmd_current.msg_id);
    }
    MQTT_ClearCommandRequest(&mqtt_cmd_current);
    memset(mqtt_cmd_buf, 0, sizeof(mqtt_cmd_buf));
    return status;
}
void L610_MQTT_LoadDefaultConfig(L610_MQTT_Config_t *config)
{
    if (config == NULL)
    {
        return;
    }

    memset(config, 0, sizeof(L610_MQTT_Config_t));
    MQTT_SafeCopy(config->apn, sizeof(config->apn), MQTT_DEFAULT_APN);
    MQTT_SafeCopy(config->host, sizeof(config->host), MQTT_BROKER_HOST);
    config->port = MQTT_BROKER_PORT;
    MQTT_SafeCopy(config->client_id, sizeof(config->client_id), MQTT_CLIENT_ID);
    MQTT_SafeCopy(config->username, sizeof(config->username), MQTT_USERNAME);
    MQTT_SafeCopy(config->password, sizeof(config->password), MQTT_PASSWORD);
    MQTT_SafeCopy(config->topic_cmd, sizeof(config->topic_cmd), MQTT_TOPIC_CMD);
    MQTT_SafeCopy(config->topic_status, sizeof(config->topic_status), MQTT_TOPIC_STATUS);
    MQTT_SafeCopy(config->topic_ack, sizeof(config->topic_ack), MQTT_TOPIC_ACK);
    MQTT_SafeCopy(config->topic_event, sizeof(config->topic_event), MQTT_TOPIC_EVENT);
    MQTT_SafeCopy(config->topic_online, sizeof(config->topic_online), MQTT_TOPIC_ONLINE);
    config->keepalive_sec = 60;
    config->clean_session = 1;
    config->use_tls = 1;
    config->default_qos = 0;
    config->auto_publish_status = 1;
    config->auto_publish_ack = 1;
}

L610_MQTT_Status_t L610_MQTT_SetConfig(const L610_MQTT_Config_t *config)
{
    if (config == NULL || config->host[0] == '\0' || config->client_id[0] == '\0' || config->port == 0)
    {
        return L610_MQTT_INVALID_PARAM;
    }

    memcpy(&mqtt_config, config, sizeof(mqtt_config));
    return L610_MQTT_OK;
}

void L610_MQTT_GetConfig(L610_MQTT_Config_t *config)
{
    if (config == NULL)
    {
        return;
    }

    memcpy(config, &mqtt_config, sizeof(mqtt_config));
}

L610_MQTT_Status_t L610_MQTT_SetServer(const char *host, uint16_t port, const char *client_id)
{
    if (host == NULL || host[0] == '\0' || port == 0)
    {
        return L610_MQTT_INVALID_PARAM;
    }

    MQTT_SafeCopy(mqtt_config.host, sizeof(mqtt_config.host), host);
    mqtt_config.port = port;

    if (client_id != NULL && client_id[0] != '\0')
    {
        MQTT_SafeCopy(mqtt_config.client_id, sizeof(mqtt_config.client_id), client_id);
    }

    return L610_MQTT_OK;
}

L610_MQTT_Status_t L610_MQTT_SetCredentials(const char *username, const char *password)
{
    if (username == NULL || password == NULL)
    {
        return L610_MQTT_INVALID_PARAM;
    }

    MQTT_SafeCopy(mqtt_config.username, sizeof(mqtt_config.username), username);
    MQTT_SafeCopy(mqtt_config.password, sizeof(mqtt_config.password), password);
    return L610_MQTT_OK;
}

L610_MQTT_Status_t L610_MQTT_SetTopics(const char *cmd_topic, const char *status_topic, const char *ack_topic, const char *event_topic, const char *online_topic)
{
    if (cmd_topic != NULL && cmd_topic[0] != '\0')
    {
        MQTT_SafeCopy(mqtt_config.topic_cmd, sizeof(mqtt_config.topic_cmd), cmd_topic);
    }

    if (status_topic != NULL && status_topic[0] != '\0')
    {
        MQTT_SafeCopy(mqtt_config.topic_status, sizeof(mqtt_config.topic_status), status_topic);
    }

    if (ack_topic != NULL && ack_topic[0] != '\0')
    {
        MQTT_SafeCopy(mqtt_config.topic_ack, sizeof(mqtt_config.topic_ack), ack_topic);
    }

    if (event_topic != NULL && event_topic[0] != '\0')
    {
        MQTT_SafeCopy(mqtt_config.topic_event, sizeof(mqtt_config.topic_event), event_topic);
    }

    if (online_topic != NULL && online_topic[0] != '\0')
    {
        MQTT_SafeCopy(mqtt_config.topic_online, sizeof(mqtt_config.topic_online), online_topic);
    }

    return L610_MQTT_OK;
}

void L610_MQTT_RegisterMessageCallback(L610_MQTT_MessageCallback_t callback)
{
    mqtt_message_callback = callback;
}

void L610_MQTT_RegisterEventCallback(L610_MQTT_EventCallback_t callback)
{
    mqtt_event_callback = callback;
}

void L610_MQTT_Init(void)
{
    memset(mqtt_cmd_buf, 0, sizeof(mqtt_cmd_buf));
    memset(mqtt_line_buf, 0, sizeof(mqtt_line_buf));
    memset(mqtt_last_line, 0, sizeof(mqtt_last_line));
    memset(mqtt_async_buf, 0, sizeof(mqtt_async_buf));
    memset(mqtt_json_buf, 0, sizeof(mqtt_json_buf));
    memset(mqtt_cmd_tx_buf, 0, sizeof(mqtt_cmd_tx_buf));
    memset(mqtt_topic_tx_buf, 0, sizeof(mqtt_topic_tx_buf));
    memset(mqtt_payload_tx_buf, 0, sizeof(mqtt_payload_tx_buf));
    memset(mqtt_last_tx, 0, sizeof(mqtt_last_tx));
    memset(mqtt_last_rx, 0, sizeof(mqtt_last_rx));
    mqtt_last_result_code = 0;
    mqtt_async_len = 0;
    mqtt_async_last_tick = 0;
    mqtt_state = MQTT_STATE_IDLE;
    mqtt_message_callback = NULL;
    mqtt_event_callback = NULL;
    MQTT_ClearCommandQueue();
    MQTT_ClearTxQueue();
    MQTT_ResetStatusTracking();
    MQTT_ClearLastMessageInternal();
    L610_MQTT_LoadDefaultConfig(&mqtt_config);
}

L610_MQTT_State_t L610_MQTT_GetState(void)
{
    return mqtt_state;
}

const char *L610_MQTT_GetStateString(void)
{
    switch (mqtt_state)
    {
    case MQTT_STATE_IDLE:
        return "IDLE";
    case MQTT_STATE_NET_READY:
        return "NET_READY";
    case MQTT_STATE_TLS_READY:
        return "TLS_READY";
    case MQTT_STATE_SOCKET_OPENING:
        return "SOCKET_OPENING";
    case MQTT_STATE_SOCKET_OPEN:
        return "SOCKET_OPEN";
    case MQTT_STATE_SESSION_CONNECTING:
        return "SESSION_CONNECTING";
    case MQTT_STATE_CONNECTED:
        return "CONNECTED";
    case MQTT_STATE_SUBSCRIBED:
        return "SUBSCRIBED";
    default:
        return "UNKNOWN";
    }
}

const char *L610_MQTT_GetStatusString(L610_MQTT_Status_t status)
{
    switch (status)
    {
    case L610_MQTT_OK:
        return "OK";
    case L610_MQTT_ERROR:
        return "ERROR";
    case L610_MQTT_TIMEOUT:
        return "TIMEOUT";
    case L610_MQTT_NO_DATA:
        return "NO_DATA";
    case L610_MQTT_INVALID_PARAM:
        return "INVALID_PARAM";
    case L610_MQTT_NOT_READY:
        return "NOT_READY";
    case L610_MQTT_PARSE_ERROR:
        return "PARSE_ERROR";
    default:
        return "UNKNOWN";
    }
}

int L610_MQTT_IsConnected(void)
{
    return (mqtt_state >= MQTT_STATE_CONNECTED) ? 1 : 0;
}

int L610_MQTT_IsReady(void)
{
    return (mqtt_state >= MQTT_STATE_SUBSCRIBED) ? 1 : 0;
}

int L610_MQTT_GetLastResultCode(void)
{
    return mqtt_last_result_code;
}

const char *L610_MQTT_GetLastTx(void)
{
    return mqtt_last_tx;
}

const char *L610_MQTT_GetLastRx(void)
{
    return mqtt_last_rx;
}

const char *L610_MQTT_GetLastStageDetail(void)
{
    return "";
}

const char *L610_MQTT_GetLastLine(void)
{
    return mqtt_last_line;
}

uint8_t L610_MQTT_GetLastStatusPublishOk(void)
{
    return mqtt_last_status_publish_ok;
}

uint32_t L610_MQTT_GetLastStatusPublishTick(void)
{
    return mqtt_last_status_publish_tick;
}

L610_MQTT_Status_t L610_MQTT_SetAPN(const char *apn)
{
    char cmd[80];

    if (apn == NULL || apn[0] == '\0')
    {
        return L610_MQTT_INVALID_PARAM;
    }

    MQTT_SafeCopy(mqtt_config.apn, sizeof(mqtt_config.apn), apn);
    snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"\r\n", mqtt_config.apn);
    MQTT_DebugPrintLine("[MQTT] Set APN: ", mqtt_config.apn);
    MQTT_LogTx(cmd);

    return (L610_SendAndWait(cmd, 1500) == L610_OK) ? L610_MQTT_OK : L610_MQTT_ERROR;
}

L610_MQTT_Status_t L610_MQTT_RequestIP(void)
{
    static const char *cmd_templates[] =
    {
        "AT+MIPCALL=1\r\n",
        "AT+MIPCALL=1,\"%s\"\r\n"
    };
    L610_MQTT_Status_t chk;
    L610_MQTT_Status_t ret;
    uint32_t i;

    MQTT_DebugPrint("[MQTT] Request IP...\r\n");
    chk = MQTT_CheckIPAlreadyReady();
    if (chk == L610_MQTT_OK)
    {
        MQTT_DebugPrint("[MQTT] IP already ready\r\n");
        return L610_MQTT_OK;
    }

    if (MQTT_MIPCALLIsStaleActive(L610_GetBuffer()) != 0)
    {
        ret = MQTT_ResetIPSession();
        if (ret == L610_MQTT_TIMEOUT)
        {
            MQTT_LogFailureContext("[MQTT] MIPCALL reset timeout");
        }
    }

    for (i = 0; i < (sizeof(cmd_templates) / sizeof(cmd_templates[0])); i++)
    {
        if (i == 0)
        {
            MQTT_SafeCopy(mqtt_cmd_tx_buf, sizeof(mqtt_cmd_tx_buf), cmd_templates[i]);
        }
        else
        {
            if (mqtt_config.apn[0] == '\0')
            {
                continue;
            }
            snprintf(mqtt_cmd_tx_buf, sizeof(mqtt_cmd_tx_buf), cmd_templates[i], mqtt_config.apn);
        }

        MQTT_LogTx(mqtt_cmd_tx_buf);
        L610_SendCmd(mqtt_cmd_tx_buf);
        ret = MQTT_WaitForKeyword("+MIPCALL:", 150000);
        if (ret == L610_MQTT_OK)
        {
            MQTT_DebugPrint("[MQTT] IP request success\r\n");
            return L610_MQTT_OK;
        }

        if (ret == L610_MQTT_TIMEOUT)
        {
            MQTT_DebugPrint("[MQTT] IP request timeout\r\n");
            MQTT_LogFailureContext("[MQTT] MIPCALL timeout");
        }
        else
        {
            MQTT_DebugPrint("[MQTT] IP request failed\r\n");
            MQTT_LogFailureContext("[MQTT] MIPCALL failed");
        }
    }

    return ret;
}

L610_MQTT_Status_t L610_MQTT_PrepareNetWithAPN(const char *apn)
{
    L610_Info_t info;

    MQTT_DebugPrint("\r\n[MQTT] Prepare network...\r\n");
    if (L610_GetInfo(&info) != L610_OK)
    {
        MQTT_DebugPrint("[MQTT] L610 info get failed\r\n");
        return L610_MQTT_ERROR;
    }

    if (!(info.at_ready && info.sim_ready && info.cgatt == 1))
    {
        MQTT_DebugPrint("[MQTT] Network not ready\r\n");
        return L610_MQTT_NOT_READY;
    }

    if (apn != NULL && apn[0] != '\0')
    {
        if (L610_MQTT_SetAPN(apn) != L610_MQTT_OK)
        {
            MQTT_DebugPrint("[MQTT] Set APN failed\r\n");
            return L610_MQTT_ERROR;
        }
    }
    else if (L610_MQTT_SetAPN(mqtt_config.apn) != L610_MQTT_OK)
    {
        MQTT_DebugPrint("[MQTT] Set APN failed\r\n");
        return L610_MQTT_ERROR;
    }

    if (L610_MQTT_RequestIP() != L610_MQTT_OK)
    {
        MQTT_DebugPrint("[MQTT] Request IP failed\r\n");
        return L610_MQTT_ERROR;
    }

    if (L610_GetInfo(&info) != L610_OK)
    {
        MQTT_DebugPrint("[MQTT] Info cache refresh failed\r\n");
    }

    mqtt_state = MQTT_STATE_NET_READY;
    MQTT_EmitEvent(MQTT_EVENT_NET_READY, mqtt_config.apn);
    MQTT_DebugPrint("[MQTT] Network ready\r\n");
    return L610_MQTT_OK;
}

L610_MQTT_Status_t L610_MQTT_PrepareNet(void)
{
    return L610_MQTT_PrepareNetWithAPN(NULL);
}

L610_MQTT_Status_t L610_MQTT_TLS_SetVersionTLS12(void)
{
    if (mqtt_config.use_tls == 0)
    {
        return L610_MQTT_OK;
    }

    MQTT_DebugPrint("[MQTT] Set TLS1.2...\r\n");
    MQTT_LogTx("AT+GTSSLVER=4");
    return (L610_SendAndWait("AT+GTSSLVER=4\r\n", 1500) == L610_OK) ? L610_MQTT_OK : L610_MQTT_ERROR;
}

L610_MQTT_Status_t L610_MQTT_TLS_LoadTrustFile(const uint8_t *data, uint32_t len)
{
    char cmd[64];

    if (data == NULL || len == 0)
    {
        return L610_MQTT_INVALID_PARAM;
    }

    snprintf(cmd, sizeof(cmd), "AT+GTSSLFILE=\"TRUSTFILE\",%lu\r\n", (unsigned long)len);
    MQTT_DebugPrint("[MQTT] Load TRUSTFILE...\r\n");
    L610_SendCmd(cmd);

    if (MQTT_ReadPromptResponse(mqtt_line_buf, sizeof(mqtt_line_buf), 3000) != L610_MQTT_OK)
    {
        return L610_MQTT_TIMEOUT;
    }

    if (strchr(mqtt_line_buf, '>') == NULL)
    {
        return L610_MQTT_ERROR;
    }

    HAL_UART_Transmit(&huart1, (uint8_t *)data, len, 5000);
    return (L610_ReadResponse(5000) == L610_OK) ? L610_MQTT_OK : L610_MQTT_ERROR;
}

L610_MQTT_Status_t L610_MQTT_TLS_ConfigMode(void)
{
    if (mqtt_config.use_tls == 0)
    {
        mqtt_state = MQTT_STATE_TLS_READY;
        return L610_MQTT_OK;
    }

    MQTT_DebugPrint("[MQTT] TLS mode uses broker-side defaults, GTSSLMODE reserved for future expansion\r\n");
    mqtt_state = MQTT_STATE_TLS_READY;
    return L610_MQTT_OK;
}

L610_MQTT_Status_t L610_MQTT_SetUser(const char *username, const char *password)
{
    char cmd[200];

    if (username == NULL || password == NULL)
    {
        return L610_MQTT_INVALID_PARAM;
    }

    MQTT_SafeCopy(mqtt_config.username, sizeof(mqtt_config.username), username);
    MQTT_SafeCopy(mqtt_config.password, sizeof(mqtt_config.password), password);

    if (mqtt_config.username[0] == '\0' && mqtt_config.password[0] == '\0')
    {
        MQTT_DebugPrint("[MQTT] MQTT user skipped (anonymous mode)\r\n");
        return L610_MQTT_OK;
    }

    snprintf(cmd, sizeof(cmd), "AT+MQTTUSER=1,\"%s\",\"%s\"\r\n", mqtt_config.username, mqtt_config.password);
    MQTT_DebugPrint("[MQTT] Set MQTT user...\r\n");
    MQTT_LogTx(cmd);
    return (L610_SendAndWait(cmd, 1500) == L610_OK) ? L610_MQTT_OK : L610_MQTT_ERROR;
}

static L610_MQTT_Status_t MQTT_TrySessionConnect(void)
{
    static const char *cmd_templates[] =
    {
        "AT+MQTTCONN=1,\"%s\"\r\n",
        "AT+MQTTCONN=1,\"%s\",%u,%u\r\n"
    };
    L610_MQTT_Status_t status;
    uint32_t i;

    if (mqtt_config.client_id[0] == '\0')
    {
        return L610_MQTT_NO_DATA;
    }

    MQTT_DebugPrint("[MQTT] Try explicit session connect...\r\n");
    mqtt_state = MQTT_STATE_SESSION_CONNECTING;

    for (i = 0; i < (sizeof(cmd_templates) / sizeof(cmd_templates[0])); i++)
    {
        if (i == 0)
        {
            snprintf(mqtt_cmd_tx_buf, sizeof(mqtt_cmd_tx_buf), cmd_templates[i], mqtt_config.client_id);
        }
        else
        {
            snprintf(mqtt_cmd_tx_buf, sizeof(mqtt_cmd_tx_buf),
                     cmd_templates[i],
                     mqtt_config.client_id,
                     (unsigned int)mqtt_config.keepalive_sec,
                     (unsigned int)(mqtt_config.clean_session ? 1 : 0));
        }

        MQTT_LogTx(mqtt_cmd_tx_buf);
        L610_SendCmd(mqtt_cmd_tx_buf);
        status = MQTT_WaitForKeyword("+MQTTCONN:", 15000);
        if (status == L610_MQTT_OK)
        {
            mqtt_state = MQTT_STATE_CONNECTED;
            MQTT_EmitEvent(MQTT_EVENT_CONNECTED, mqtt_config.client_id);
            MQTT_DebugPrint("[MQTT] Explicit session connect success\r\n");
            return L610_MQTT_OK;
        }
    }

    mqtt_state = MQTT_STATE_SOCKET_OPEN;
    MQTT_DebugPrint("[MQTT] Explicit session connect unresolved on current firmware\r\n");
    return L610_MQTT_NO_DATA;
}

L610_MQTT_Status_t L610_MQTT_Connect(void)
{
    L610_MQTT_Status_t status;

    if (mqtt_state < MQTT_STATE_NET_READY)
    {
        MQTT_DebugPrint("[MQTT] Connect denied: net not ready\r\n");
        return L610_MQTT_NOT_READY;
    }

    if (mqtt_state >= MQTT_STATE_CONNECTED)
    {
        return L610_MQTT_OK;
    }

    status = L610_MQTT_TLS_SetVersionTLS12();
    if (status != L610_MQTT_OK)
    {
        MQTT_DebugPrint("[MQTT] TLS version set failed\r\n");
        return status;
    }

    status = L610_MQTT_TLS_ConfigMode();
    if (status != L610_MQTT_OK)
    {
        MQTT_DebugPrint("[MQTT] TLS mode config failed\r\n");
        return status;
    }

    status = L610_MQTT_SetUser(mqtt_config.username, mqtt_config.password);
    if (status != L610_MQTT_OK)
    {
        MQTT_DebugPrint("[MQTT] MQTTUSER failed\r\n");
        return status;
    }

    if (mqtt_config.use_tls != 0)
    {
        snprintf(mqtt_cmd_tx_buf, sizeof(mqtt_cmd_tx_buf),
                 "AT+MQTTOPEN=1,\"%s\",%u,%u,%u,2\r\n",
                 mqtt_config.host,
                 (unsigned int)mqtt_config.port,
                 (unsigned int)(mqtt_config.clean_session ? 1 : 0),
                 (unsigned int)mqtt_config.keepalive_sec);
    }
    else
    {
        snprintf(mqtt_cmd_tx_buf, sizeof(mqtt_cmd_tx_buf),
                 "AT+MQTTOPEN=1,\"%s\",%u,%u,%u\r\n",
                 mqtt_config.host,
                 (unsigned int)mqtt_config.port,
                 (unsigned int)(mqtt_config.clean_session ? 1 : 0),
                 (unsigned int)mqtt_config.keepalive_sec);
    }

    MQTT_DebugPrint("[MQTT] Connecting broker...\r\n");
    MQTT_DebugPrintLine("  Host: ", mqtt_config.host);
    MQTT_DebugPrintLine("  ClientID: ", mqtt_config.client_id);
    MQTT_DebugPrintLine("  Username: ", mqtt_config.username);

    mqtt_state = MQTT_STATE_SOCKET_OPENING;
    MQTT_LogTx(mqtt_cmd_tx_buf);
    L610_SendCmd(mqtt_cmd_tx_buf);
    status = MQTT_WaitForKeyword("+MQTTOPEN:", 60000);
    if (status == L610_MQTT_OK)
    {
        mqtt_state = MQTT_STATE_SOCKET_OPEN;
        MQTT_DebugPrint("[MQTT] Broker socket open\r\n");
        status = MQTT_TrySessionConnect();
        if (status == L610_MQTT_NO_DATA)
        {
            mqtt_state = MQTT_STATE_CONNECTED;
            MQTT_DebugPrint("[MQTT] MQTTOPEN is used as session-ready state on current firmware\r\n");
            MQTT_EmitEvent(MQTT_EVENT_CONNECTED, mqtt_config.host);
            return L610_MQTT_OK;
        }

        if (status == L610_MQTT_OK)
        {
            return L610_MQTT_OK;
        }

        mqtt_state = MQTT_STATE_SOCKET_OPEN;
        MQTT_LogFailureContext("[MQTT] Session connect failed");
    }
    else
    {
        MQTT_DebugPrint("[MQTT] Broker connect failed\r\n");
        MQTT_LogFailureContext("[MQTT] MQTTOPEN failed");
        MQTT_BestEffortCloseSession();
    }

    return status;
}
L610_MQTT_Status_t L610_MQTT_Subscribe(const char *topic, uint8_t qos)
{
    L610_MQTT_Status_t status;

    if (topic == NULL || topic[0] == '\0')
    {
        return L610_MQTT_INVALID_PARAM;
    }

    if (mqtt_state < MQTT_STATE_CONNECTED)
    {
        MQTT_DebugPrint("[MQTT] Subscribe denied: not connected\r\n");
        return L610_MQTT_NOT_READY;
    }

    snprintf(mqtt_cmd_tx_buf, sizeof(mqtt_cmd_tx_buf), "AT+MQTTSUB=1,\"%s\",%u\r\n", topic, (unsigned int)qos);
    MQTT_DebugPrintLine("[MQTT] Subscribe topic: ", topic);

    MQTT_LogTx(mqtt_cmd_tx_buf);
    L610_SendCmd(mqtt_cmd_tx_buf);
    status = MQTT_WaitForKeyword("+MQTTSUB:", 60000);
    if (status == L610_MQTT_OK)
    {
        mqtt_state = MQTT_STATE_SUBSCRIBED;
        MQTT_EmitEvent(MQTT_EVENT_SUBSCRIBED, topic);
        MQTT_DebugPrint("[MQTT] Subscribe success\r\n");
    }
    else
    {
        MQTT_LogFailureContext("[MQTT] Subscribe failed");
    }

    return status;
}

L610_MQTT_Status_t L610_MQTT_SubscribeCmd(void)
{
    L610_MQTT_Status_t status;
#if MQTT_FEATURE_DEBUG_WILDCARD_SUB
    L610_MQTT_Status_t wildcard_status;
#endif

    status = L610_MQTT_Subscribe(mqtt_config.topic_cmd, mqtt_config.default_qos);
    if (status != L610_MQTT_OK)
    {
        return status;
    }

#if MQTT_FEATURE_DEBUG_WILDCARD_SUB
    wildcard_status = L610_MQTT_Subscribe(MQTT_DEBUG_WILDCARD_TOPIC, mqtt_config.default_qos);
    if (wildcard_status == L610_MQTT_OK)
    {
        MQTT_DebugPrintLine("[MQTT] Debug wildcard subscribed: ", MQTT_DEBUG_WILDCARD_TOPIC);
    }
    else
    {
        MQTT_DebugPrintLine("[MQTT] Debug wildcard subscribe skipped: ",
                            L610_MQTT_GetStatusString(wildcard_status));
    }
#endif

    return status;
}

L610_MQTT_Status_t L610_MQTT_PublishEx(const char *topic, const char *payload, uint8_t qos, uint8_t retain)
{
    if (topic == NULL || payload == NULL || topic[0] == '\0')
    {
        return L610_MQTT_INVALID_PARAM;
    }

    if (mqtt_state < MQTT_STATE_CONNECTED)
    {
        MQTT_DebugPrint("[MQTT] Publish denied: not connected\r\n");
        return L610_MQTT_NOT_READY;
    }

    return MQTT_QueuePublish(topic, payload, qos, retain);
}

L610_MQTT_Status_t L610_MQTT_Publish(const char *topic, const char *payload)
{
    return L610_MQTT_PublishEx(topic, payload, mqtt_config.default_qos, 0);
}

L610_MQTT_Status_t L610_MQTT_BuildStatusJson(char *out_buf, uint16_t buf_size)
{
    App_DeviceStatus_t status;
    char escaped_status[MQTT_JSON_BUF_SIZE];
    char escaped_wind_direction[APP_STATUS_FIELD_LEN * 2U];
    char escaped_rain_level[APP_STATUS_FIELD_LEN * 2U];
    char escaped_last_error[APP_STATUS_TEXT_LEN];
    char escaped_wind_tx[APP_STATUS_TEXT_LEN];
    char escaped_wind_rx[APP_STATUS_TEXT_LEN];
    char escaped_air_frame[APP_STATUS_TEXT_LEN];
    char escaped_mqtt_stage[APP_STATUS_FIELD_LEN * 2U];
    char timestamp[MQTT_TIMESTAMP_BUF_SIZE];

    if (out_buf == NULL || buf_size == 0)
    {
        return L610_MQTT_INVALID_PARAM;
    }

    App_FillDeviceStatus(&status);
    MQTT_FormatTimestamp(timestamp, sizeof(timestamp));
    MQTT_EscapeJson(status.state_text, escaped_status, sizeof(escaped_status));
    MQTT_EscapeJson(status.weather.wind_direction_text, escaped_wind_direction, sizeof(escaped_wind_direction));
    MQTT_EscapeJson(status.weather.rain_level_text, escaped_rain_level, sizeof(escaped_rain_level));
    MQTT_EscapeJson(status.weather.sensor_last_error, escaped_last_error, sizeof(escaped_last_error));
    MQTT_EscapeJson(status.weather.wind_last_tx_hex, escaped_wind_tx, sizeof(escaped_wind_tx));
    MQTT_EscapeJson(status.weather.wind_last_rx_hex, escaped_wind_rx, sizeof(escaped_wind_rx));
    MQTT_EscapeJson(status.last_cj702_frame_hex, escaped_air_frame, sizeof(escaped_air_frame));
    MQTT_EscapeJson(status.l610_mqtt_stage, escaped_mqtt_stage, sizeof(escaped_mqtt_stage));

    snprintf(out_buf, buf_size,
             "{\"type\":\"status\",\"device_id\":\"%s\",\"online\":%u,\"relay1\":%u,\"relay2\":%u,"
             "\"rssi\":%d,\"operator\":\"%s\",\"ip\":\"%s\",\"state\":\"%s\",\"tick\":%lu,"
             "\"protocol_profile\":\"airport_pad_v1\",\"timestamp\":\"%s\","
             "\"net\":{\"rssi\":%d,\"operator\":\"%s\",\"ip\":\"%s\"},"
             "\"pad\":{\"left_state\":\"%s\",\"right_state\":\"%s\",\"ready\":%u,\"occupied\":%u,\"mode\":\"%s\"},"
             "\"weather\":{\"wind_speed\":%.2f,\"wind_direction\":%.2f,\"wind_speed_raw\":%u,\"wind_direction_raw\":%u,"
             "\"rain_adc_raw\":%u,\"wind_direction_text\":\"%s\",\"rain_level_text\":\"%s\","
             "\"temperature\":%.2f,\"humidity\":%.2f,\"pressure\":%.2f,\"visibility\":%.2f,"
             "\"capabilities\":{\"wind\":%u,\"air\":%u,\"rain\":%u,\"pressure\":%u,\"visibility\":%u},"
             "\"rain_detected\":%u,\"rain_value\":%.2f,"
             "\"pm25\":%.2f,\"pm10\":%.2f,\"co2\":%.2f,\"tvoc\":%.4f,\"ch2o\":%.4f,"
             "\"sensor_status\":{\"wind_online\":%u,\"air_online\":%u,\"rain_online\":%u,\"failure_count\":%u,"
             "\"last_ok_tick\":%lu,\"last_error\":\"%s\",\"wind_last_tx_hex\":\"%s\","
             "\"wind_last_rx_hex\":\"%s\",\"air_last_frame_hex\":\"%s\",\"l610_state\":\"%s\"}}}",
             status.device_id,
             (unsigned int)status.online,
             (unsigned int)status.relay1,
             (unsigned int)status.relay2,
             status.net.rssi,
             status.net.operator_name,
             status.net.ip,
             escaped_status,
             (unsigned long)status.tick,
             timestamp,
             status.net.rssi,
             status.net.operator_name,
             status.net.ip,
             status.pad.left_state,
             status.pad.right_state,
             (unsigned int)status.pad.ready,
             (unsigned int)status.pad.occupied,
             status.pad.mode,
             (double)status.weather.wind_speed,
             (double)status.weather.wind_direction,
             (unsigned int)status.weather.wind_speed_raw,
             (unsigned int)status.weather.wind_direction_raw,
             (unsigned int)status.weather.rain_adc_raw,
             escaped_wind_direction,
             escaped_rain_level,
             (double)status.weather.temperature,
             (double)status.weather.humidity,
             (double)status.weather.pressure,
             (double)status.weather.visibility,
             (unsigned int)status.weather.capability_wind,
             (unsigned int)status.weather.capability_air,
             (unsigned int)status.weather.capability_rain,
             (unsigned int)status.weather.capability_pressure,
             (unsigned int)status.weather.capability_visibility,
             (unsigned int)status.weather.rain_detected,
             (double)status.weather.rain_value,
             (double)status.weather.pm25,
             (double)status.weather.pm10,
             (double)status.weather.co2,
             (double)status.weather.tvoc,
             (double)status.weather.ch2o,
             (unsigned int)status.weather.wind_online,
             (unsigned int)status.weather.air_online,
             (unsigned int)status.weather.rain_online,
             (unsigned int)status.weather.sensor_failure_count,
             (unsigned long)status.weather.sensor_last_ok_tick,
             escaped_last_error,
             escaped_wind_tx,
             escaped_wind_rx,
             escaped_air_frame,
             escaped_mqtt_stage);

    return L610_MQTT_OK;
}

L610_MQTT_Status_t L610_MQTT_PublishStatus(void)
{
    if (L610_MQTT_BuildStatusJson(mqtt_json_buf, sizeof(mqtt_json_buf)) != L610_MQTT_OK)
    {
        mqtt_last_status_publish_ok = 0U;
        mqtt_last_status_publish_tick = HAL_GetTick();
        return L610_MQTT_ERROR;
    }
    {
        L610_MQTT_Status_t publish_status = L610_MQTT_PublishEx(mqtt_config.topic_status, mqtt_json_buf, mqtt_config.default_qos, 1U);
        mqtt_last_status_publish_ok = (uint8_t)((publish_status == L610_MQTT_OK) ? 1U : 0U);
        mqtt_last_status_publish_tick = HAL_GetTick();
        return publish_status;
    }
}

L610_MQTT_Status_t L610_MQTT_RequestStatusRefresh(void)
{
    MQTT_RequestStatusPublishInternal();
    return (L610_MQTT_IsConnected() != 0) ? L610_MQTT_OK : L610_MQTT_NOT_READY;
}

L610_MQTT_Status_t L610_MQTT_PublishOnlineState(uint8_t online)
{
    char timestamp[MQTT_TIMESTAMP_BUF_SIZE];

    MQTT_FormatTimestamp(timestamp, sizeof(timestamp));
    snprintf(mqtt_json_buf, sizeof(mqtt_json_buf),
             "{\"device_id\":\"%s\",\"online\":%u,\"timestamp\":\"%s\"}",
             mqtt_config.client_id,
             (unsigned int)(online != 0U ? 1U : 0U),
             timestamp);

    return L610_MQTT_PublishEx(mqtt_config.topic_online, mqtt_json_buf, mqtt_config.default_qos, 1U);
}

L610_MQTT_Status_t L610_MQTT_PublishEvent(const char *event_name, const char *detail)
{
    char event_escaped[MQTT_TOPIC_BUF_SIZE * 2];
    char detail_escaped[MQTT_JSON_BUF_SIZE];
    char timestamp[MQTT_TIMESTAMP_BUF_SIZE];

    if (event_name == NULL || event_name[0] == '\0')
    {
        return L610_MQTT_INVALID_PARAM;
    }

    MQTT_EscapeJson(event_name, event_escaped, sizeof(event_escaped));
    MQTT_EscapeJson(detail != NULL ? detail : "", detail_escaped, sizeof(detail_escaped));
    MQTT_FormatTimestamp(timestamp, sizeof(timestamp));
    snprintf(mqtt_json_buf, sizeof(mqtt_json_buf),
             "{\"type\":\"event\",\"device_id\":\"%s\",\"event\":\"%s\",\"detail\":\"%s\",\"tick\":%lu,\"timestamp\":\"%s\"}",
             mqtt_config.client_id,
             event_escaped,
             detail_escaped,
             (unsigned long)HAL_GetTick(),
             timestamp);

    return L610_MQTT_PublishEx(mqtt_config.topic_event, mqtt_json_buf, mqtt_config.default_qos, 0U);
}

L610_MQTT_Status_t L610_MQTT_PublishCommandResponse(const char *command, const char *result, const char *detail)
{
    char cmd_escaped[MQTT_PAYLOAD_BUF_SIZE];
    char result_escaped[64];
    char detail_escaped[MQTT_JSON_BUF_SIZE];
    char timestamp[MQTT_TIMESTAMP_BUF_SIZE];
    const char *response_cmd;
    const char *response_msg_id;
    const char *response_device_id;
    uint8_t relay1_on;
    uint8_t relay2_on;

    if (command == NULL || result == NULL)
    {
        return L610_MQTT_INVALID_PARAM;
    }

    response_cmd = (mqtt_cmd_current.request_cmd[0] != '\0') ? mqtt_cmd_current.request_cmd : command;
    response_msg_id = (mqtt_cmd_current.msg_id[0] != '\0') ? mqtt_cmd_current.msg_id : "";
    response_device_id = (mqtt_cmd_current.device_id[0] != '\0') ? mqtt_cmd_current.device_id : mqtt_config.client_id;
    MQTT_EscapeJson(response_cmd, cmd_escaped, sizeof(cmd_escaped));
    MQTT_EscapeJson(result, result_escaped, sizeof(result_escaped));
    MQTT_EscapeJson(detail != NULL ? detail : "", detail_escaped, sizeof(detail_escaped));
    MQTT_FormatTimestamp(timestamp, sizeof(timestamp));
    relay1_on = (Relay_GetState(RELAY1) == RELAY_ON) ? 1U : 0U;
    relay2_on = (Relay_GetState(RELAY2) == RELAY_ON) ? 1U : 0U;

    snprintf(mqtt_json_buf, sizeof(mqtt_json_buf),
             "{\"type\":\"ack\",\"msg_id\":\"%s\",\"device_id\":\"%s\",\"cmd\":\"%s\",\"result\":\"%s\",\"detail\":\"%s\",\"relay1\":%u,\"relay2\":%u,\"timestamp\":\"%s\"}",
             response_msg_id,
             response_device_id,
             cmd_escaped,
             result_escaped,
             detail_escaped,
             relay1_on,
             relay2_on,
             timestamp);

    return L610_MQTT_PublishEx(mqtt_config.topic_ack, mqtt_json_buf, mqtt_config.default_qos, 0U);
}

L610_MQTT_Status_t L610_MQTT_Close(void)
{
    L610_MQTT_Status_t status;
    L610_MQTT_Status_t publish_status;

    if (mqtt_state < MQTT_STATE_SOCKET_OPEN)
    {
        MQTT_ClearTxQueue();
        MQTT_ResetStatusTracking();
        return L610_MQTT_OK;
    }

    if (L610_MQTT_IsConnected() != 0)
    {
        publish_status = L610_MQTT_PublishOnlineState(0U);
        if (publish_status == L610_MQTT_OK)
        {
            MQTT_StartNextQueuedPublish();
            publish_status = MQTT_WaitForKeywordOrOk("+MQTTPUB:", 3000);
            MQTT_FinishQueuedPublish(publish_status);
        }
    }

    MQTT_LogTx("AT+MQTTCLOSE=1");
    L610_SendCmd("AT+MQTTCLOSE=1\r\n");
    status = MQTT_WaitForKeywordOrOk("+MQTTCLOSE:", 3000);
    if (status != L610_MQTT_OK)
    {
        MQTT_LogFailureContext("[MQTT] Close failed");
        return L610_MQTT_ERROR;
    }

    mqtt_state = MQTT_STATE_NET_READY;
    MQTT_ClearTxQueue();
    MQTT_ResetStatusTracking();
    MQTT_EmitEvent(MQTT_EVENT_DISCONNECTED, mqtt_config.host);
    return L610_MQTT_OK;
}
L610_MQTT_Status_t L610_MQTT_ParseIncoming(const char *line, char *topic_out, uint16_t topic_size, char *payload_out, uint16_t payload_size)
{
    const char *prefix;
    const char *p;
    char *endptr;
    long value;
    long qos;
    const char *topic_start;
    const char *topic_end;
    const char *payload_start;
    const char *payload_end;

    if (line == NULL || topic_out == NULL || payload_out == NULL || topic_size == 0 || payload_size == 0)
    {
        return L610_MQTT_INVALID_PARAM;
    }

    topic_out[0] = '\0';
    payload_out[0] = '\0';

    prefix = MQTT_FindIncomingPrefix(line);
    if (prefix == NULL)
    {
        return L610_MQTT_PARSE_ERROR;
    }

    p = strchr(prefix, ':');
    if (p == NULL)
    {
        return L610_MQTT_PARSE_ERROR;
    }

    p++;
    while (*p == ' ' || *p == '\t')
    {
        p++;
    }

    value = strtol(p, &endptr, 10);
    if (endptr == p)
    {
        return L610_MQTT_PARSE_ERROR;
    }

    p = endptr;
    while (*p == ' ' || *p == '\t' || *p == ',')
    {
        p++;
    }

    qos = strtol(p, &endptr, 10);
    if (endptr == p)
    {
        return L610_MQTT_PARSE_ERROR;
    }

    (void)value;
    (void)qos;
    p = endptr;
    while (*p == ' ' || *p == '\t' || *p == ',')
    {
        p++;
    }

    if (*p == '"')
    {
        topic_start = p + 1;
        topic_end = strchr(topic_start, '"');
        if (topic_end == NULL)
        {
            return L610_MQTT_PARSE_ERROR;
        }
        p = topic_end + 1;
    }
    else
    {
        topic_start = p;
        topic_end = strchr(p, ',');
        if (topic_end == NULL)
        {
            return L610_MQTT_PARSE_ERROR;
        }
        p = topic_end;
    }

    MQTT_CopySegmentTrim(topic_start, topic_end, topic_out, topic_size);

    while (*p == ' ' || *p == '\t' || *p == ',')
    {
        p++;
    }

    payload_start = p;
    payload_end = line + strlen(line);
    MQTT_CopySegmentTrim(payload_start, payload_end, payload_out, payload_size);
    return L610_MQTT_OK;
}

L610_MQTT_Status_t L610_MQTT_ProcessLine(const char *line)
{
    char topic[MQTT_TOPIC_BUF_SIZE];
    char payload[MQTT_PAYLOAD_BUF_SIZE];
    char command[MQTT_CMD_BUF_SIZE];
    char *p;
    char *endptr;
    long client_id;
    long qos;
    MQTT_CommandRequest_t request;
    L610_MQTT_Status_t command_status;

    if (line == NULL)
    {
        return L610_MQTT_INVALID_PARAM;
    }

    MQTT_SetLastLine(line);
    if (line[0] == '\0')
    {
        return L610_MQTT_NO_DATA;
    }

    if (strstr(line, "+MQTTOPEN:") != NULL)
    {
        if (MQTT_CheckKeywordResult("+MQTTOPEN:", line) == L610_MQTT_OK)
        {
            mqtt_state = MQTT_STATE_SOCKET_OPEN;
        }
        return L610_MQTT_OK;
    }

    if (strstr(line, "+MQTTCONN:") != NULL)
    {
        if (MQTT_CheckKeywordResult("+MQTTCONN:", line) == L610_MQTT_OK)
        {
            mqtt_state = MQTT_STATE_CONNECTED;
            MQTT_RequestStatusPublishInternal();
            MQTT_EmitEvent(MQTT_EVENT_CONNECTED, mqtt_config.client_id);
        }
        return L610_MQTT_OK;
    }

    if (strstr(line, "+MQTTSUB:") != NULL)
    {
        if (MQTT_CheckKeywordResult("+MQTTSUB:", line) == L610_MQTT_OK)
        {
            mqtt_state = MQTT_STATE_SUBSCRIBED;
            if (mqtt_subscribe_refresh_done == 0U && mqtt_subscribe_refresh_pending == 0U)
            {
                mqtt_subscribe_refresh_pending = 1U;
                mqtt_subscribe_refresh_tick = HAL_GetTick() + MQTT_SUBSCRIBE_REFRESH_DELAY_MS;
            }
            MQTT_RequestOnlinePublishInternal(1U);
            MQTT_RequestStatusPublishInternal();
            MQTT_EmitEvent(MQTT_EVENT_SUBSCRIBED, mqtt_config.topic_cmd);
        }
        return L610_MQTT_OK;
    }

    if (strstr(line, "+MQTTPUB:") != NULL)
    {
        if (MQTT_CheckKeywordResult("+MQTTPUB:", line) == L610_MQTT_OK)
        {
            MQTT_FinishQueuedPublish(L610_MQTT_OK);
        }
        else
        {
            MQTT_LogFailureContext("[MQTT] Publish result error");
            MQTT_FinishQueuedPublish(L610_MQTT_ERROR);
        }
        return L610_MQTT_OK;
    }

    if (strstr(line, "+MQTTCLOSE:") != NULL)
    {
        mqtt_state = MQTT_STATE_NET_READY;
        MQTT_ClearTxQueue();
        MQTT_ResetStatusTracking();
        MQTT_EmitEvent(MQTT_EVENT_DISCONNECTED, mqtt_config.host);
        return L610_MQTT_OK;
    }

    if (strcmp(line, "OK") == 0)
    {
        if (mqtt_tx_inflight != 0U)
        {
            MQTT_FinishQueuedPublish(L610_MQTT_OK);
            return L610_MQTT_OK;
        }

        return L610_MQTT_NO_DATA;
    }

    if ((strstr(line, "ERROR") != NULL || strstr(line, "+CME ERROR") != NULL) &&
        mqtt_tx_inflight != 0U)
    {
        MQTT_LogFailureContext("[MQTT] Publish command error");
        MQTT_FinishQueuedPublish(L610_MQTT_ERROR);
        return L610_MQTT_ERROR;
    }

    if (MQTT_FindIncomingPrefix(line) != NULL)
    {
        topic[0] = '\0';
        payload[0] = '\0';
        if (L610_MQTT_ParseIncoming(line, topic, sizeof(topic), payload, sizeof(payload)) != L610_MQTT_OK)
        {
            MQTT_DebugPrintLine("[MQTT] Incoming parse failed: ", line);
            if (MQTT_FindKnownCommandToken(line, command, sizeof(command)) != 0)
            {
                MQTT_DebugPrintLine("[MQTT] Parsed CMD fallback: ", command);
                if (L610_MQTT_SetIncomingCommand(command) != L610_MQTT_OK)
                {
                    MQTT_DebugPrint("[MQTT] Queue CMD failed\r\n");
                    return L610_MQTT_ERROR;
                }
                return L610_MQTT_OK;
            }

            return L610_MQTT_PARSE_ERROR;
        }

        p = strchr(line, ':');
        if (p == NULL)
        {
            return L610_MQTT_PARSE_ERROR;
        }

        p++;
        while (*p == ' ' || *p == '\t')
        {
            p++;
        }

        client_id = strtol(p, &endptr, 10);
        if (endptr == p)
        {
            client_id = 0;
        }

        p = endptr;
        while (*p == ' ' || *p == '\t' || *p == ',')
        {
            p++;
        }

        qos = strtol(p, &endptr, 10);
        if (endptr == p)
        {
            qos = 0;
        }

        MQTT_StoreMessage((int)client_id, (int)qos, topic, payload, line);
        if (strcmp(topic, mqtt_config.topic_cmd) == 0)
        {
            MQTT_DeferNextPublish(MQTT_POST_RX_GUARD_MS);
            MQTT_DebugPrintLine("[MQTT] Raw CMD payload: ", payload);
            command_status = MQTT_ParseIncomingCommandPayload(payload, &request);
            if (command_status == L610_MQTT_OK)
            {
                MQTT_DebugPrintLine("[MQTT] Parsed CMD: ", request.protocol_cmd);
                if (MQTT_FindRecentMsgId(request.msg_id) != 0U)
                {
                    MQTT_DebugPrintLine("[MQTT] Duplicate msg_id: ", request.msg_id);
                    mqtt_cmd_current = request;
                    if (mqtt_config.auto_publish_ack && L610_MQTT_IsConnected())
                    {
                        L610_MQTT_PublishCommandResponse(request.protocol_cmd, "success", "DUPLICATE");
                    }
                    MQTT_ClearCommandRequest(&mqtt_cmd_current);
                }
                else if (MQTT_EnqueueCommandRequest(&request) != L610_MQTT_OK)
                {
                    MQTT_DebugPrint("[MQTT] Queue CMD failed\r\n");
                }
            }
            else if (command_status == L610_MQTT_INVALID_PARAM)
            {
                MQTT_DebugPrint("[MQTT] CMD rejected: invalid param\r\n");
                if ((request.device_id[0] == '\0' || strcmp(request.device_id, mqtt_config.client_id) == 0) &&
                    mqtt_config.auto_publish_ack &&
                    L610_MQTT_IsConnected())
                {
                    mqtt_cmd_current = request;
                    L610_MQTT_PublishCommandResponse((request.protocol_cmd[0] != '\0') ? request.protocol_cmd : "INVALID",
                                                    "error",
                                                    "INVALID_PARAM");
                    MQTT_ClearCommandRequest(&mqtt_cmd_current);
                }
            }
            else
            {
                MQTT_DebugPrint("[MQTT] Parsed CMD empty\r\n");
            }
        }
        return L610_MQTT_OK;
    }

    return L610_MQTT_NO_DATA;
}

L610_MQTT_Status_t L610_MQTT_HandleCommand(char *cmd)
{
    ProtocolStatus_t protocol_status;
    char cmd_copy[MQTT_CMD_BUF_SIZE];
    L610_MQTT_Status_t mqtt_status;
    const char *detail;
    const char *protocol_detail;

    if (cmd == NULL)
    {
        return L610_MQTT_INVALID_PARAM;
    }

    MQTT_SafeCopy(cmd_copy, sizeof(cmd_copy), cmd);
    MQTT_DebugPrintLine("[MQTT] Handle CMD: ", cmd_copy);
    MQTT_LogRelaySnapshot("[MQTT] Relay before: ");
    protocol_status = Protocol_Parse(cmd);
    MQTT_DebugPrintLine("[MQTT] Protocol status: ", MQTT_GetProtocolStatusString(protocol_status));
    protocol_detail = Protocol_GetLastStatusText();
    if (protocol_detail != NULL && protocol_detail[0] != '\0')
    {
        MQTT_DebugPrintLine("[MQTT] Protocol detail: ", protocol_detail);
    }
    MQTT_LogRelaySnapshot("[MQTT] Relay after: ");

    detail = "OK";
    if (protocol_status == PROTOCOL_UNKNOWN_CMD)
    {
        detail = "UNKNOWN_CMD";
    }
    else if (protocol_status == PROTOCOL_INVALID_PARAM)
    {
        detail = "INVALID_PARAM";
    }
    else if (protocol_status == PROTOCOL_EXEC_ERROR)
    {
        detail = "EXEC_ERROR";
    }

    if (mqtt_config.auto_publish_ack && L610_MQTT_IsConnected())
    {
        mqtt_status = L610_MQTT_PublishCommandResponse(cmd_copy,
                                                       (protocol_status == PROTOCOL_OK) ? "success" : "error",
                                                       detail);
        if (mqtt_status != L610_MQTT_OK)
        {
            MQTT_DebugPrint("[MQTT] Publish command ack failed\r\n");
        }
    }

    if (protocol_status != PROTOCOL_OK)
    {
        MQTT_EmitEvent(MQTT_EVENT_COMMAND_REJECTED, cmd_copy);
        return L610_MQTT_ERROR;
    }

    if (mqtt_config.auto_publish_status && L610_MQTT_IsConnected())
    {
        mqtt_status_priority_requested = 1U;
        mqtt_command_fast_until = HAL_GetTick() + MQTT_COMMAND_FAST_WINDOW_MS;
    }

    MQTT_EmitEvent(MQTT_EVENT_COMMAND_HANDLED, cmd_copy);
    return L610_MQTT_OK;
}

L610_MQTT_Status_t L610_MQTT_SetIncomingCommand(const char *cmd)
{
    if (cmd == NULL)
    {
        return L610_MQTT_INVALID_PARAM;
    }

    return MQTT_EnqueueLegacyCommand(cmd);
}

uint8_t L610_MQTT_HasPendingMessage(void)
{
    return mqtt_message_pending;
}

L610_MQTT_Status_t L610_MQTT_GetLastMessage(L610_MQTT_Message_t *out_message)
{
    if (out_message == NULL)
    {
        return L610_MQTT_INVALID_PARAM;
    }

    if (mqtt_message_pending == 0)
    {
        return L610_MQTT_NO_DATA;
    }

    memcpy(out_message, &mqtt_last_message, sizeof(mqtt_last_message));
    return L610_MQTT_OK;
}

void L610_MQTT_ClearLastMessage(void)
{
    MQTT_ClearLastMessageInternal();
}

void L610_MQTT_Task(void)
{
    uint8_t ch;

    while (HAL_UART_Receive(&huart1, &ch, 1, MQTT_RX_ASYNC_TIMEOUT_MS) == HAL_OK)
    {
        mqtt_async_last_tick = HAL_GetTick();
        if (mqtt_async_len < sizeof(mqtt_async_buf) - 1)
        {
            mqtt_async_buf[mqtt_async_len++] = (char)ch;
            mqtt_async_buf[mqtt_async_len] = '\0';
        }
        else
        {
            MQTT_ProcessAsyncLineBuffer();
            mqtt_async_buf[mqtt_async_len++] = (char)ch;
            mqtt_async_buf[mqtt_async_len] = '\0';
        }

        if (ch == '\n')
        {
            MQTT_ProcessAsyncLineBuffer();
        }
    }

    if (mqtt_async_len > 0 && mqtt_async_last_tick != 0)
    {
        if ((HAL_GetTick() - mqtt_async_last_tick) >= MQTT_RX_INTERBYTE_TIMEOUT_MS)
        {
            MQTT_ProcessAsyncLineBuffer();
        }
    }

    if (mqtt_async_len == 0 && mqtt_cmd_pending != 0)
    {
        MQTT_ProcessPendingCommand();
    }

    MQTT_RunLinkMaintenance();
    MQTT_RunOnlinePublisher();
    MQTT_RunRealtimeStatusPublisher();
    MQTT_HandlePublishTimeout();
    MQTT_StartNextQueuedPublish();
}
