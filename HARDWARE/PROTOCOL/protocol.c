#include "protocol.h"
#include "relay.h"
#include "l610.h"
#include "l610_mqtt.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define PROTOCOL_RESPONSE_BUF_SIZE 1024
#define PROTOCOL_ATRAW_TIMEOUT_MS  5000
#define PROTOCOL_ATRAW_IDLE_MS      300

static char protocol_response_buf[PROTOCOL_RESPONSE_BUF_SIZE];
static char protocol_status_text[32];

static int Protocol_StrEqual(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

static void Protocol_TrimLineEnd(char *str)
{
    int len;

    if (str == NULL)
        return;

    len = strlen(str);
    while (len > 0 &&
          (str[len - 1] == '\r' ||
           str[len - 1] == '\n' ||
           str[len - 1] == ' '))
    {
        str[len - 1] = '\0';
        len--;
    }
}

static void Protocol_SetResponse(const char *response)
{
    if (response == NULL)
    {
        protocol_response_buf[0] = '\0';
        return;
    }

    strncpy(protocol_response_buf, response, sizeof(protocol_response_buf) - 1);
    protocol_response_buf[sizeof(protocol_response_buf) - 1] = '\0';
}

static void Protocol_SetStatusText(const char *status_text)
{
    if (status_text == NULL)
    {
        protocol_status_text[0] = '\0';
        return;
    }

    strncpy(protocol_status_text, status_text, sizeof(protocol_status_text) - 1);
    protocol_status_text[sizeof(protocol_status_text) - 1] = '\0';
}

static void Protocol_SetSummaryResponse(const char *summary)
{
    Protocol_SetResponse(summary);
}

static void Protocol_SetMQTTResponse(const char *label, L610_MQTT_Status_t status)
{
    snprintf(protocol_response_buf, sizeof(protocol_response_buf),
             "%s=%s\r\nSTATE=%s",
             label,
             L610_MQTT_GetStatusString(status),
             L610_MQTT_GetStateString());
}

static ProtocolStatus_t Protocol_HandleATRaw(char *cmd)
{
    char *payload;
    char *timeout_end;
    unsigned long timeout_ms;
    uint8_t timeout_explicit;
    char at_cmd[256];
    L610_Status_t l610_status;

    payload = cmd + 5;
    while (*payload == ' ')
    {
        payload++;
    }

    if (*payload == '\0')
    {
        Protocol_SetStatusText("INVALID_PARAM");
        Protocol_SetResponse("Usage: ATRAW [timeout_ms] AT+CMD");
        return PROTOCOL_INVALID_PARAM;
    }

    timeout_ms = PROTOCOL_ATRAW_TIMEOUT_MS;
    timeout_explicit = 0;
    timeout_end = payload;
    while (*timeout_end >= '0' && *timeout_end <= '9')
    {
        timeout_end++;
    }

    if (timeout_end > payload && *timeout_end == ' ')
    {
        timeout_ms = strtoul(payload, NULL, 10);
        timeout_explicit = 1;
        payload = timeout_end + 1;
        while (*payload == ' ')
        {
            payload++;
        }
    }

    if (*payload == '\0')
    {
        Protocol_SetStatusText("INVALID_PARAM");
        Protocol_SetResponse("Usage: ATRAW [timeout_ms] AT+CMD");
        return PROTOCOL_INVALID_PARAM;
    }

    if (strncmp(payload, "AT", 2) != 0)
    {
        snprintf(at_cmd, sizeof(at_cmd), "AT%s", payload);
    }
    else
    {
        strncpy(at_cmd, payload, sizeof(at_cmd) - 1);
        at_cmd[sizeof(at_cmd) - 1] = '\0';
    }

    if (!timeout_explicit)
    {
        if (strstr(at_cmd, "AT+MQTTOPEN") != NULL)
        {
            timeout_ms = 60000;
        }
        else if (strstr(at_cmd, "AT+MIPCALL=") != NULL)
        {
            timeout_ms = 150000;
        }
    }

    l610_status = L610_SendRawCommand(at_cmd, (uint32_t)timeout_ms, PROTOCOL_ATRAW_IDLE_MS);
    if (l610_status == L610_OK)
    {
        Protocol_SetStatusText("OK");
    }
    else if (l610_status == L610_TIMEOUT)
    {
        Protocol_SetStatusText("TIMEOUT");
    }
    else
    {
        Protocol_SetStatusText("ERROR");
    }

    if (L610_GetBuffer()[0] != '\0')
    {
        Protocol_SetResponse(L610_GetBuffer());
    }
    else if (l610_status == L610_TIMEOUT)
    {
        Protocol_SetResponse("TIMEOUT");
    }
    else
    {
        Protocol_SetResponse("NO RESPONSE");
    }

    return (l610_status == L610_OK) ? PROTOCOL_OK : PROTOCOL_EXEC_ERROR;
}

static ProtocolStatus_t Protocol_HandleSelfTest(void)
{
    L610_Status_t status;

    status = L610_SelfTest();
    Protocol_SetStatusText((status == L610_OK) ? "OK" : "ERROR");
    Protocol_SetSummaryResponse((status == L610_OK) ? "SELFTEST=OK" : "SELFTEST=ERROR");
    return (status == L610_OK) ? PROTOCOL_OK : PROTOCOL_EXEC_ERROR;
}

static ProtocolStatus_t Protocol_HandleNetInit(void)
{
    L610_MQTT_Status_t status;

    status = L610_MQTT_PrepareNet();
    Protocol_SetStatusText(L610_MQTT_GetStatusString(status));
    Protocol_SetMQTTResponse("NETINIT", status);
    return (status == L610_MQTT_OK) ? PROTOCOL_OK : PROTOCOL_EXEC_ERROR;
}

static ProtocolStatus_t Protocol_HandleMQTTInit(void)
{
    L610_MQTT_Status_t status;

    if (L610_MQTT_GetState() < MQTT_STATE_NET_READY)
    {
        Protocol_SetStatusText("NOT_READY");
        Protocol_SetResponse("Run NETINIT first");
        return PROTOCOL_EXEC_ERROR;
    }

    status = L610_MQTT_Connect();
    if (status == L610_MQTT_OK)
    {
        status = L610_MQTT_SubscribeCmd();
    }

    Protocol_SetStatusText(L610_MQTT_GetStatusString(status));
    Protocol_SetMQTTResponse("MQTTINIT", status);
    return (status == L610_MQTT_OK) ? PROTOCOL_OK : PROTOCOL_EXEC_ERROR;
}

static ProtocolStatus_t Protocol_HandleMQTTClose(void)
{
    L610_MQTT_Status_t status;

    status = L610_MQTT_Close();
    Protocol_SetStatusText(L610_MQTT_GetStatusString(status));
    Protocol_SetMQTTResponse("MQTTCLOSE", status);
    return (status == L610_MQTT_OK) ? PROTOCOL_OK : PROTOCOL_EXEC_ERROR;
}

ProtocolStatus_t Protocol_Parse(char *cmd)
{
    if (cmd == NULL)
    {
        Protocol_SetStatusText("INVALID_PARAM");
        return PROTOCOL_INVALID_PARAM;
    }

    Protocol_SetStatusText("OK");
    Protocol_SetResponse("");
    Protocol_TrimLineEnd(cmd);

    if (strncmp(cmd, "ATRAW", 5) == 0)
    {
        return Protocol_HandleATRaw(cmd);
    }
    else if (Protocol_StrEqual(cmd, "SELFTEST"))
    {
        return Protocol_HandleSelfTest();
    }
    else if (Protocol_StrEqual(cmd, "NETINIT"))
    {
        return Protocol_HandleNetInit();
    }
    else if (Protocol_StrEqual(cmd, "MQTTINIT"))
    {
        return Protocol_HandleMQTTInit();
    }
    else if (Protocol_StrEqual(cmd, "MQTTCLOSE"))
    {
        return Protocol_HandleMQTTClose();
    }
    else if (Protocol_StrEqual(cmd, "R1_ON"))
    {
        Relay_On(RELAY1);
        Protocol_SetStatusText("OK");
        Protocol_SetResponse("OK");
        return PROTOCOL_OK;
    }
    else if (Protocol_StrEqual(cmd, "R1_OFF"))
    {
        Relay_Off(RELAY1);
        Protocol_SetStatusText("OK");
        Protocol_SetResponse("OK");
        return PROTOCOL_OK;
    }
    else if (Protocol_StrEqual(cmd, "R2_ON"))
    {
        Relay_On(RELAY2);
        Protocol_SetStatusText("OK");
        Protocol_SetResponse("OK");
        return PROTOCOL_OK;
    }
    else if (Protocol_StrEqual(cmd, "R2_OFF"))
    {
        Relay_Off(RELAY2);
        Protocol_SetStatusText("OK");
        Protocol_SetResponse("OK");
        return PROTOCOL_OK;
    }
    else if (Protocol_StrEqual(cmd, "ALL_ON"))
    {
        Relay_AllOn();
        Protocol_SetStatusText("OK");
        Protocol_SetResponse("OK");
        return PROTOCOL_OK;
    }
    else if (Protocol_StrEqual(cmd, "ALL_OFF"))
    {
        Relay_AllOff();
        Protocol_SetStatusText("OK");
        Protocol_SetResponse("OK");
        return PROTOCOL_OK;
    }
    else if (Protocol_StrEqual(cmd, "PAD_OPEN"))
    {
        Relay_AllOn();
        Protocol_SetStatusText("OK");
        Protocol_SetResponse("OK");
        return PROTOCOL_OK;
    }
    else if (Protocol_StrEqual(cmd, "PAD_CLOSE"))
    {
        Relay_AllOff();
        Protocol_SetStatusText("OK");
        Protocol_SetResponse("OK");
        return PROTOCOL_OK;
    }
    else if (Protocol_StrEqual(cmd, "PAD_STOP"))
    {
        Protocol_SetStatusText("OK");
        Protocol_SetResponse("OK");
        return PROTOCOL_OK;
    }
    else if (Protocol_StrEqual(cmd, "STATUS"))
    {
        Protocol_SetStatusText("OK");
        Protocol_GetStatusString(protocol_response_buf, sizeof(protocol_response_buf));
        return PROTOCOL_OK;
    }
    else if (Protocol_StrEqual(cmd, "QUERY_PAD_STATUS"))
    {
        Protocol_SetStatusText("OK");
        Protocol_GetStatusString(protocol_response_buf, sizeof(protocol_response_buf));
        return PROTOCOL_OK;
    }

    Protocol_SetStatusText("UNKNOWN_CMD");
    return PROTOCOL_UNKNOWN_CMD;
}

void Protocol_GetStatusString(char *out_buf, uint16_t buf_size)
{
    L610_Info_t info;
    const char *r1;
    const char *r2;

    if (out_buf == NULL || buf_size == 0)
    {
        return;
    }

    r1 = (Relay_GetState(RELAY1) == RELAY_ON) ? "ON" : "OFF";
    r2 = (Relay_GetState(RELAY2) == RELAY_ON) ? "ON" : "OFF";

    if (L610_GetCachedInfo(&info) == L610_OK)
    {
        snprintf(out_buf, buf_size,
                 "R1=%s,R2=%s,AT=%d,SIM=%d,RSSI=%d,CREG=%d,CGREG=%d,CGATT=%d,OP=%s,IP=%s",
                 r1,
                 r2,
                 info.at_ready,
                 info.sim_ready,
                 info.rssi,
                 info.creg,
                 info.cgreg,
                 info.cgatt,
                 info.operator_name,
                 info.ip_addr);
    }
    else
    {
        snprintf(out_buf, buf_size,
                 "R1=%s,R2=%s,NET=NO_CACHE",
                 r1, r2);
    }
}

const char *Protocol_GetLastResponse(void)
{
    return protocol_response_buf;
}

const char *Protocol_GetLastStatusText(void)
{
    return protocol_status_text;
}
