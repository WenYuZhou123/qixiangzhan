#include "l610.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void L610_SendCmdLine(const char *cmd);

static uint8_t l610_rx_buf[L610_RX_BUF_SIZE];
static uint16_t l610_rx_len = 0;
static uint8_t l610_ch = 0;
static L610_Info_t l610_cached_info;
static uint8_t l610_cached_info_valid = 0;
static uint32_t l610_cached_info_tick = 0;

/* 内部调试打印 */
static void L610_DebugPrint(const char *str)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), 1000);
}

/* 打印当前接收缓冲区 */
static void L610_PrintBuffer(void)
{
    HAL_UART_Transmit(&huart2, l610_rx_buf, l610_rx_len, 1000);
    L610_DebugPrint("\r\n");
}

static void L610_PrepareCommandTx(void)
{
    L610_FlushRx();
}

static void L610_UpdateInfoCache(const L610_Info_t *info)
{
    if (info == NULL)
    {
        return;
    }

    memcpy(&l610_cached_info, info, sizeof(l610_cached_info));
    l610_cached_info_valid = 1;
    l610_cached_info_tick = HAL_GetTick();
}

static L610_Status_t L610_SendSimpleCommand(const char *cmd, uint32_t timeout)
{
    L610_PrepareCommandTx();
    L610_SendCmdLine(cmd);
    return L610_ReadResponse(timeout);
}

static void L610_SendCmdLine(const char *cmd)
{
    uint16_t len;

    if (cmd == NULL)
    {
        return;
    }

    len = (uint16_t)strlen(cmd);
    if (len == 0)
    {
        return;
    }

    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, len, 1000);
    if (len < 2 || cmd[len - 2] != '\r' || cmd[len - 1] != '\n')
    {
        HAL_UART_Transmit(&huart1, (uint8_t *)"\r\n", 2, 1000);
    }
}

void L610_Init(void)
{
    L610_ClearBuffer();
    L610_ClearInfoCache();
}

void L610_ClearBuffer(void)
{
    memset(l610_rx_buf, 0, sizeof(l610_rx_buf));
    l610_rx_len = 0;
}

void L610_FlushRx(void)
{
    uint32_t start;

    L610_ClearBuffer();
    start = HAL_GetTick();
    while (HAL_GetTick() - start < 10)
    {
        if (HAL_UART_Receive(&huart1, &l610_ch, 1, 1) == HAL_OK)
        {
            start = HAL_GetTick();
        }
        else
        {
            break;
        }
    }

    L610_ClearBuffer();
}

void L610_SendCmd(const char *cmd)
{
    if (cmd == NULL)
    {
        return;
    }

    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 1000);
}

L610_Status_t L610_Sync(uint8_t disable_echo)
{
    uint8_t attempt;
    L610_Status_t status = L610_TIMEOUT;

    for (attempt = 0; attempt < 3; attempt++)
    {
        status = L610_SendSimpleCommand("AT", 1500);
        if (status == L610_OK)
        {
            break;
        }

        HAL_Delay(200);
    }

    if (status != L610_OK)
    {
        return status;
    }

    if (disable_echo)
    {
        for (attempt = 0; attempt < 2; attempt++)
        {
            status = L610_SendSimpleCommand("ATE0", 1500);
            if (status == L610_OK)
            {
                break;
            }

            HAL_Delay(100);
        }
    }

    return status;
}

L610_Status_t L610_ReadResponse(uint32_t timeout)
{
    uint32_t start = HAL_GetTick();

    L610_ClearBuffer();

    while (HAL_GetTick() - start < timeout)
    {
        if (HAL_UART_Receive(&huart1, &l610_ch, 1, 50) == HAL_OK)
        {
            if (l610_rx_len < L610_RX_BUF_SIZE - 1)
            {
                l610_rx_buf[l610_rx_len++] = l610_ch;
                l610_rx_buf[l610_rx_len] = '\0';
            }

            if (strstr((char *)l610_rx_buf, "\r\nOK\r\n") != NULL ||
                strcmp((char *)l610_rx_buf, "OK\r\n") == 0)
            {
                return L610_OK;
            }

            if (strstr((char *)l610_rx_buf, "ERROR") != NULL)
            {
                return L610_ERROR;
            }
        }
    }

    return L610_TIMEOUT;
}

L610_Status_t L610_ReadRawResponse(uint32_t timeout, uint32_t idle_timeout)
{
    uint32_t start;
    uint32_t last_rx_tick;
    uint8_t got_any;

    start = HAL_GetTick();
    last_rx_tick = start;
    got_any = 0;
    L610_ClearBuffer();

    while (HAL_GetTick() - start < timeout)
    {
        if (HAL_UART_Receive(&huart1, &l610_ch, 1, 50) == HAL_OK)
        {
            got_any = 1;
            last_rx_tick = HAL_GetTick();
            if (l610_rx_len < L610_RX_BUF_SIZE - 1)
            {
                l610_rx_buf[l610_rx_len++] = l610_ch;
                l610_rx_buf[l610_rx_len] = '\0';
            }
        }
        else if (got_any && (HAL_GetTick() - last_rx_tick >= idle_timeout))
        {
            break;
        }
    }

    if (!got_any)
    {
        return L610_TIMEOUT;
    }

    if (strstr((char *)l610_rx_buf, "ERROR") != NULL || strstr((char *)l610_rx_buf, "+CME ERROR") != NULL)
    {
        return L610_ERROR;
    }

    return L610_OK;
}

L610_Status_t L610_SendAndWait(const char *cmd, uint32_t timeout)
{
    L610_PrepareCommandTx();
    L610_SendCmd(cmd);
    return L610_ReadResponse(timeout);
}

L610_Status_t L610_SendRawCommand(const char *cmd, uint32_t timeout, uint32_t idle_timeout)
{
    L610_PrepareCommandTx();
    L610_SendCmdLine(cmd);
    return L610_ReadRawResponse(timeout, idle_timeout);
}

char* L610_GetBuffer(void)
{
    return (char *)l610_rx_buf;
}

L610_Status_t L610_SetEchoOff(void)
{
    L610_DebugPrint("Send: ATE0\r\n");
    L610_PrepareCommandTx();
    L610_SendCmd("ATE0\r\n");

    L610_Status_t status = L610_ReadResponse(1500);

    L610_DebugPrint("Recv:\r\n");
    HAL_UART_Transmit(&huart2, l610_rx_buf, l610_rx_len, 1000);
    L610_DebugPrint("\r\n");

    return status;
}

L610_Status_t L610_TestAT(void)
{
    L610_DebugPrint("Send: AT\r\n");
    L610_PrepareCommandTx();
    L610_SendCmd("AT\r\n");

    L610_Status_t status = L610_ReadResponse(1500);

    L610_DebugPrint("Recv:\r\n");
    HAL_UART_Transmit(&huart2, l610_rx_buf, l610_rx_len, 1000);
    L610_DebugPrint("\r\n");

    return status;
}

L610_Status_t L610_CheckSIM(void)
{
    L610_PrepareCommandTx();
    L610_SendCmd("AT+CPIN?\r\n");
    return L610_ReadResponse(1500);
}

L610_Status_t L610_GetCSQ(int *rssi, int *ber)
{
    char *p;

    if (rssi == NULL || ber == NULL)
    {
        return L610_ERROR;
    }

    L610_PrepareCommandTx();
    L610_SendCmd("AT+CSQ\r\n");
    L610_Status_t status = L610_ReadResponse(1500);
    if (status != L610_OK)
        return status;

    p = strstr((char *)l610_rx_buf, "+CSQ:");
    if (p != NULL)
    {
        sscanf(p, "+CSQ: %d,%d", rssi, ber);
        return L610_OK;
    }

    return L610_ERROR;
}

L610_Status_t L610_CheckCREG(uint8_t *stat)
{
    int n = 0, s = 0;
    char *p;

    if (stat == NULL)
    {
        return L610_ERROR;
    }

    L610_PrepareCommandTx();
    L610_SendCmd("AT+CREG?\r\n");
    L610_Status_t status = L610_ReadResponse(1500);
    if (status != L610_OK)
        return status;

    p = strstr((char *)l610_rx_buf, "+CREG:");
    if (p != NULL)
    {
        sscanf(p, "+CREG: %d,%d", &n, &s);
        *stat = (uint8_t)s;
        return L610_OK;
    }

    return L610_ERROR;
}

L610_Status_t L610_CheckCGREG(uint8_t *stat)
{
    int n = 0, s = 0;
    char *p;

    if (stat == NULL)
    {
        return L610_ERROR;
    }

    L610_PrepareCommandTx();
    L610_SendCmd("AT+CGREG?\r\n");
    L610_Status_t status = L610_ReadResponse(1500);
    if (status != L610_OK)
        return status;

    p = strstr((char *)l610_rx_buf, "+CGREG:");
    if (p != NULL)
    {
        sscanf(p, "+CGREG: %d,%d", &n, &s);
        *stat = (uint8_t)s;
        return L610_OK;
    }

    return L610_ERROR;
}

L610_Status_t L610_CheckCGATT(uint8_t *attached)
{
    char *p;
    int att = 0;

    if (attached == NULL)
    {
        return L610_ERROR;
    }

    L610_PrepareCommandTx();
    L610_SendCmd("AT+CGATT?\r\n");
    L610_Status_t status = L610_ReadResponse(1500);
    if (status != L610_OK)
        return status;

    p = strstr((char *)l610_rx_buf, "+CGATT:");
    if (p != NULL)
    {
        sscanf(p, "+CGATT: %d", &att);
        *attached = (uint8_t)att;
        return L610_OK;
    }

    return L610_ERROR;
}

L610_Status_t L610_GetOperator(char *operator_name, uint16_t buf_size)
{
    char *first_quote;
    char *second_quote;
    uint16_t len;

    if (operator_name == NULL || buf_size == 0)
    {
        return L610_ERROR;
    }

    memset(operator_name, 0, buf_size);

    L610_PrepareCommandTx();
    L610_SendCmd("AT+COPS?\r\n");
    L610_Status_t status = L610_ReadResponse(2000);
    if (status != L610_OK)
        return status;

    first_quote = strchr((char *)l610_rx_buf, '"');
    if (first_quote == NULL)
    {
        return L610_ERROR;
    }

    second_quote = strchr(first_quote + 1, '"');
    if (second_quote == NULL)
    {
        return L610_ERROR;
    }

    len = (uint16_t)(second_quote - first_quote - 1);
    if (len >= buf_size)
    {
        len = buf_size - 1;
    }

    memcpy(operator_name, first_quote + 1, len);
    operator_name[len] = '\0';

    return L610_OK;
}

L610_Status_t L610_GetIPAddress(char *ip_addr, uint16_t buf_size)
{
    char *first_quote;
    char *second_quote;
    uint16_t len;

    if (ip_addr == NULL || buf_size == 0)
    {
        return L610_ERROR;
    }

    memset(ip_addr, 0, buf_size);

    L610_PrepareCommandTx();
    L610_SendCmd("AT+CGPADDR\r\n");
    L610_Status_t status = L610_ReadResponse(2000);
    if (status != L610_OK)
        return status;

    first_quote = strchr((char *)l610_rx_buf, '"');
    if (first_quote == NULL)
    {
        return L610_ERROR;
    }

    second_quote = strchr(first_quote + 1, '"');
    if (second_quote == NULL)
    {
        return L610_ERROR;
    }

    len = (uint16_t)(second_quote - first_quote - 1);
    if (len >= buf_size)
    {
        len = buf_size - 1;
    }

    memcpy(ip_addr, first_quote + 1, len);
    ip_addr[len] = '\0';

    return L610_OK;
}

L610_Status_t L610_GetInfo(L610_Info_t *info)
{
    if (info == NULL)
    {
        return L610_ERROR;
    }

    memset(info, 0, sizeof(L610_Info_t));

    if (L610_TestAT() == L610_OK)
    {
        info->at_ready = 1;
    }
    else
    {
        info->at_ready = 0;
        return L610_ERROR;
    }

    if (L610_CheckSIM() == L610_OK)
    {
        info->sim_ready = 1;
    }
    else
    {
        info->sim_ready = 0;
        return L610_ERROR;
    }

    if (L610_GetCSQ(&info->rssi, &info->ber) != L610_OK)
    {
        return L610_ERROR;
    }

    if (L610_CheckCREG(&info->creg) != L610_OK)
    {
        return L610_ERROR;
    }

    if (L610_CheckCGREG(&info->cgreg) != L610_OK)
    {
        return L610_ERROR;
    }

    if (L610_CheckCGATT(&info->cgatt) != L610_OK)
    {
        return L610_ERROR;
    }

    if (L610_GetOperator(info->operator_name, sizeof(info->operator_name)) != L610_OK)
    {
        strcpy(info->operator_name, "UNKNOWN");
    }

    if (L610_GetIPAddress(info->ip_addr, sizeof(info->ip_addr)) != L610_OK)
    {
        strcpy(info->ip_addr, "0.0.0.0");
    }

    L610_UpdateInfoCache(info);
    return L610_OK;
}

L610_Status_t L610_GetCachedInfo(L610_Info_t *info)
{
    if (info == NULL)
    {
        return L610_ERROR;
    }

    if (l610_cached_info_valid == 0)
    {
        memset(info, 0, sizeof(L610_Info_t));
        return L610_ERROR;
    }

    memcpy(info, &l610_cached_info, sizeof(L610_Info_t));
    return L610_OK;
}

void L610_ClearInfoCache(void)
{
    memset(&l610_cached_info, 0, sizeof(l610_cached_info));
    l610_cached_info_valid = 0;
    l610_cached_info_tick = 0;
}

uint32_t L610_GetCachedInfoAgeMs(void)
{
    if (l610_cached_info_valid == 0)
    {
        return 0xFFFFFFFFUL;
    }

    return HAL_GetTick() - l610_cached_info_tick;
}

void L610_PrintInfo(const L610_Info_t *info)
{
    char msg[192];

    if (info == NULL)
    {
        L610_DebugPrint("L610 info is NULL\r\n");
        return;
    }

    snprintf(msg, sizeof(msg),
             "AT=%d, SIM=%d, RSSI=%d, BER=%d, CREG=%d, CGREG=%d, CGATT=%d, OP=%s, IP=%s\r\n",
             info->at_ready,
             info->sim_ready,
             info->rssi,
             info->ber,
             info->creg,
             info->cgreg,
             info->cgatt,
             info->operator_name,
             info->ip_addr);

    L610_DebugPrint(msg);
}

L610_Status_t L610_SelfTest(void)
{
    L610_Info_t info;
    L610_Status_t result = L610_OK;

    L610_DebugPrint("\r\n===== L610 SELF TEST =====\r\n");

    if (L610_SetEchoOff() == L610_OK)
    {
        L610_DebugPrint("Echo off success\r\n");
    }
    else
    {
        L610_DebugPrint("Echo off failed\r\n");
        result = L610_ERROR;
    }

    if (L610_GetInfo(&info) == L610_OK)
    {
        L610_DebugPrint("L610 info get success\r\n");
        L610_PrintInfo(&info);

        if (L610_CheckSIM() == L610_OK)
        {
            L610_DebugPrint("SIM raw:\r\n");
            L610_PrintBuffer();
        }

        if (L610_GetCSQ(&info.rssi, &info.ber) == L610_OK)
        {
            L610_DebugPrint("CSQ raw:\r\n");
            L610_PrintBuffer();
        }

        if (L610_CheckCREG(&info.creg) == L610_OK)
        {
            L610_DebugPrint("CREG raw:\r\n");
            L610_PrintBuffer();
        }

        if (L610_CheckCGREG(&info.cgreg) == L610_OK)
        {
            L610_DebugPrint("CGREG raw:\r\n");
            L610_PrintBuffer();
        }

        if (L610_CheckCGATT(&info.cgatt) == L610_OK)
        {
            L610_DebugPrint("CGATT raw:\r\n");
            L610_PrintBuffer();
        }

        if (L610_GetOperator(info.operator_name, sizeof(info.operator_name)) == L610_OK)
        {
            L610_DebugPrint("COPS raw:\r\n");
            L610_PrintBuffer();
        }

        if (L610_GetIPAddress(info.ip_addr, sizeof(info.ip_addr)) == L610_OK)
        {
            L610_DebugPrint("CGPADDR raw:\r\n");
            L610_PrintBuffer();
        }
    }
    else
    {
        L610_DebugPrint("L610 info get failed\r\n");
        result = L610_ERROR;
    }

    L610_DebugPrint("===== SELF TEST END =====\r\n\r\n");

    return result;
}
