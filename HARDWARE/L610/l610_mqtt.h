#ifndef __L610_MQTT_H
#define __L610_MQTT_H

#include "main.h"
#include "l610.h"
#include <stdint.h>

#define MQTT_DEFAULT_APN         "3gnet"
#define MQTT_BROKER_HOST         "rc11adc1.ala.cn-hangzhou.emqxsl.cn"
#define MQTT_BROKER_PORT         8883
#define MQTT_CLIENT_ID           "relay_h743_001"
#define MQTT_USERNAME            "h743"
#define MQTT_PASSWORD            "123456"

#define MQTT_TOPIC_BASE          "device/" MQTT_CLIENT_ID
#define MQTT_TOPIC_CMD           MQTT_TOPIC_BASE "/down/cmd"
#define MQTT_TOPIC_STATUS        MQTT_TOPIC_BASE "/up/status"
#define MQTT_TOPIC_ACK           MQTT_TOPIC_BASE "/up/ack"
#define MQTT_TOPIC_EVENT         MQTT_TOPIC_BASE "/up/event"
#define MQTT_TOPIC_ONLINE        MQTT_TOPIC_BASE "/up/online"
#define MQTT_TOPIC_DEBUG_ALL     MQTT_TOPIC_BASE "/#"

#define MQTT_CMD_SET_R1          "set_r1"
#define MQTT_CMD_SET_R2          "set_r2"
#define MQTT_CMD_SET_ALL         "set_all"
#define MQTT_CMD_QUERY_STATUS    "query_status"
#define MQTT_CMD_PAD_OPEN        "pad_open"
#define MQTT_CMD_PAD_CLOSE       "pad_close"
#define MQTT_CMD_PAD_STOP        "pad_stop"
#define MQTT_CMD_QUERY_PAD_STATUS "query_pad_status"
#define MQTT_SUPPORTED_COMMANDS  "set_r1,set_r2,set_all,query_status,pad_open,pad_close,pad_stop,query_pad_status"
#define MQTT_LEGACY_COMMANDS     "R1_ON,R1_OFF,R2_ON,R2_OFF,ALL_ON,ALL_OFF,STATUS"

#define MQTT_FEATURE_VERBOSE_LOG          1U
#define MQTT_FEATURE_DEBUG_WILDCARD_SUB   0U
#define MQTT_FEATURE_KEEPALIVE_STATUS     1U
#define MQTT_KEEPALIVE_STATUS_INTERVAL_MS 20000U

#define MQTT_CMD_BUF_SIZE        256
#define MQTT_STATUS_BUF_SIZE     1024
#define MQTT_AT_BUF_SIZE         512
#define MQTT_HOST_BUF_SIZE       96
#define MQTT_CLIENT_ID_BUF_SIZE  64
#define MQTT_USER_BUF_SIZE       64
#define MQTT_PASSWORD_BUF_SIZE   64
#define MQTT_TOPIC_BUF_SIZE      128
#define MQTT_PAYLOAD_BUF_SIZE    384
#define MQTT_JSON_BUF_SIZE       1024
#define MQTT_TX_CMD_BUF_SIZE     1024

typedef enum
{
    L610_MQTT_OK = 0,
    L610_MQTT_ERROR,
    L610_MQTT_TIMEOUT,
    L610_MQTT_NO_DATA,
    L610_MQTT_INVALID_PARAM,
    L610_MQTT_NOT_READY,
    L610_MQTT_PARSE_ERROR
} L610_MQTT_Status_t;

typedef enum
{
    MQTT_STATE_IDLE = 0,
    MQTT_STATE_NET_READY,
    MQTT_STATE_TLS_READY,
    MQTT_STATE_SOCKET_OPENING,
    MQTT_STATE_SOCKET_OPEN,
    MQTT_STATE_SESSION_CONNECTING,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_SUBSCRIBED
} L610_MQTT_State_t;

typedef enum
{
    MQTT_EVENT_NONE = 0,
    MQTT_EVENT_NET_READY,
    MQTT_EVENT_CONNECTED,
    MQTT_EVENT_DISCONNECTED,
    MQTT_EVENT_SUBSCRIBED,
    MQTT_EVENT_MESSAGE,
    MQTT_EVENT_PUBLISH_OK,
    MQTT_EVENT_PUBLISH_ERROR,
    MQTT_EVENT_COMMAND_HANDLED,
    MQTT_EVENT_COMMAND_REJECTED
} L610_MQTT_Event_t;

typedef struct
{
    char apn[32];
    char host[MQTT_HOST_BUF_SIZE];
    uint16_t port;
    char client_id[MQTT_CLIENT_ID_BUF_SIZE];
    char username[MQTT_USER_BUF_SIZE];
    char password[MQTT_PASSWORD_BUF_SIZE];
    char topic_cmd[MQTT_TOPIC_BUF_SIZE];
    char topic_status[MQTT_TOPIC_BUF_SIZE];
    char topic_ack[MQTT_TOPIC_BUF_SIZE];
    char topic_event[MQTT_TOPIC_BUF_SIZE];
    char topic_online[MQTT_TOPIC_BUF_SIZE];
    uint16_t keepalive_sec;
    uint8_t clean_session;
    uint8_t use_tls;
    uint8_t default_qos;
    uint8_t auto_publish_status;
    uint8_t auto_publish_ack;
} L610_MQTT_Config_t;

typedef struct
{
    int client_id;
    int qos;
    char topic[MQTT_TOPIC_BUF_SIZE];
    char payload[MQTT_PAYLOAD_BUF_SIZE];
    char raw_line[MQTT_AT_BUF_SIZE];
    uint32_t tick;
} L610_MQTT_Message_t;

typedef void (*L610_MQTT_MessageCallback_t)(const L610_MQTT_Message_t *message);
typedef void (*L610_MQTT_EventCallback_t)(L610_MQTT_Event_t event, const char *detail);

void L610_MQTT_Init(void);
void L610_MQTT_LoadDefaultConfig(L610_MQTT_Config_t *config);
L610_MQTT_Status_t L610_MQTT_SetConfig(const L610_MQTT_Config_t *config);
void L610_MQTT_GetConfig(L610_MQTT_Config_t *config);
L610_MQTT_Status_t L610_MQTT_SetServer(const char *host, uint16_t port, const char *client_id);
L610_MQTT_Status_t L610_MQTT_SetCredentials(const char *username, const char *password);
L610_MQTT_Status_t L610_MQTT_SetTopics(const char *cmd_topic, const char *status_topic, const char *ack_topic, const char *event_topic, const char *online_topic);
void L610_MQTT_RegisterMessageCallback(L610_MQTT_MessageCallback_t callback);
void L610_MQTT_RegisterEventCallback(L610_MQTT_EventCallback_t callback);

L610_MQTT_State_t L610_MQTT_GetState(void);
const char *L610_MQTT_GetStateString(void);
const char *L610_MQTT_GetStatusString(L610_MQTT_Status_t status);
int L610_MQTT_IsConnected(void);
int L610_MQTT_IsReady(void);
int L610_MQTT_GetLastResultCode(void);
const char *L610_MQTT_GetLastTx(void);
const char *L610_MQTT_GetLastRx(void);
const char *L610_MQTT_GetLastLine(void);
uint8_t L610_MQTT_GetLastStatusPublishOk(void);
uint32_t L610_MQTT_GetLastStatusPublishTick(void);
const char *L610_MQTT_GetLastStageDetail(void);

L610_MQTT_Status_t L610_MQTT_SetAPN(const char *apn);
L610_MQTT_Status_t L610_MQTT_RequestIP(void);
L610_MQTT_Status_t L610_MQTT_PrepareNet(void);
L610_MQTT_Status_t L610_MQTT_PrepareNetWithAPN(const char *apn);

L610_MQTT_Status_t L610_MQTT_TLS_SetVersionTLS12(void);
L610_MQTT_Status_t L610_MQTT_TLS_LoadTrustFile(const uint8_t *data, uint32_t len);
L610_MQTT_Status_t L610_MQTT_TLS_ConfigMode(void);

L610_MQTT_Status_t L610_MQTT_SetUser(const char *username, const char *password);
L610_MQTT_Status_t L610_MQTT_Connect(void);
L610_MQTT_Status_t L610_MQTT_Subscribe(const char *topic, uint8_t qos);
L610_MQTT_Status_t L610_MQTT_SubscribeCmd(void);
L610_MQTT_Status_t L610_MQTT_Publish(const char *topic, const char *payload);
L610_MQTT_Status_t L610_MQTT_PublishEx(const char *topic, const char *payload, uint8_t qos, uint8_t retain);
L610_MQTT_Status_t L610_MQTT_BuildStatusJson(char *out_buf, uint16_t buf_size);
L610_MQTT_Status_t L610_MQTT_RequestStatusRefresh(void);
L610_MQTT_Status_t L610_MQTT_PublishStatus(void);
L610_MQTT_Status_t L610_MQTT_PublishOnlineState(uint8_t online);
L610_MQTT_Status_t L610_MQTT_PublishEvent(const char *event_name, const char *detail);
L610_MQTT_Status_t L610_MQTT_PublishCommandResponse(const char *command, const char *result, const char *detail);
L610_MQTT_Status_t L610_MQTT_Close(void);

L610_MQTT_Status_t L610_MQTT_ParseIncoming(const char *line, char *topic_out, uint16_t topic_size, char *payload_out, uint16_t payload_size);
L610_MQTT_Status_t L610_MQTT_ProcessLine(const char *line);
L610_MQTT_Status_t L610_MQTT_HandleCommand(char *cmd);
void L610_MQTT_Task(void);

L610_MQTT_Status_t L610_MQTT_SetIncomingCommand(const char *cmd);
uint8_t L610_MQTT_HasPendingMessage(void);
L610_MQTT_Status_t L610_MQTT_GetLastMessage(L610_MQTT_Message_t *out_message);
void L610_MQTT_ClearLastMessage(void);

#endif
