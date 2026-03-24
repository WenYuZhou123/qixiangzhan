#ifndef __L610_H
#define __L610_H

#include "main.h"
#include "usart.h"
#include <stdint.h>

#define L610_RX_BUF_SIZE       8192
#define L610_OPERATOR_NAME_LEN   32
#define L610_IP_ADDR_LEN         32
#define L610_DEBUG_ENABLE         1U

typedef enum
{
    L610_OK = 0,
    L610_ERROR,
    L610_TIMEOUT
} L610_Status_t;

typedef enum
{
    L610_OWNER_NONE = 0,
    L610_OWNER_DIAG,
    L610_OWNER_MQTT,
    L610_OWNER_PROTOCOL
} L610_Owner_t;

typedef enum
{
    L610_SESSION_OK = 0,
    L610_SESSION_BUSY,
    L610_SESSION_INVALID
} L610_SessionStatus_t;

typedef struct
{
    uint8_t at_ready;                         // 1: AT正常
    uint8_t sim_ready;                        // 1: SIM正常
    int rssi;                                 // 信号强度
    int ber;                                  // 误码率
    uint8_t creg;                             // 电路域注册状态
    uint8_t cgreg;                            // 分组域注册状态
    uint8_t cgatt;                            // 数据附着状态
    char operator_name[L610_OPERATOR_NAME_LEN]; // 运营商名
    char ip_addr[L610_IP_ADDR_LEN];             // IP地址
} L610_Info_t;

void L610_Init(void);
void L610_ClearBuffer(void);
void L610_FlushRx(void);
L610_SessionStatus_t L610_BeginSession(L610_Owner_t owner);
void L610_EndSession(L610_Owner_t owner);
L610_Owner_t L610_GetOwner(void);
const char *L610_GetOwnerString(void);
L610_Status_t L610_Sync(uint8_t disable_echo);
void L610_SendCmd(const char *cmd);
L610_Status_t L610_ReadResponse(uint32_t timeout);
L610_Status_t L610_ReadRawResponse(uint32_t timeout, uint32_t idle_timeout);
L610_Status_t L610_SendAndWait(const char *cmd, uint32_t timeout);
L610_Status_t L610_SendRawCommand(const char *cmd, uint32_t timeout, uint32_t idle_timeout);
char* L610_GetBuffer(void);

L610_Status_t L610_SetEchoOff(void);
L610_Status_t L610_SetVerboseError(uint8_t mode);
L610_Status_t L610_TestAT(void);
L610_Status_t L610_CheckSIM(void);
L610_Status_t L610_GetCSQ(int *rssi, int *ber);
L610_Status_t L610_CheckCREG(uint8_t *stat);
L610_Status_t L610_CheckCGREG(uint8_t *stat);
L610_Status_t L610_CheckCGATT(uint8_t *attached);
L610_Status_t L610_GetOperator(char *operator_name, uint16_t buf_size);
L610_Status_t L610_GetIPAddress(char *ip_addr, uint16_t buf_size);

L610_Status_t L610_GetInfo(L610_Info_t *info);
L610_Status_t L610_GetCachedInfo(L610_Info_t *info);
void L610_ClearInfoCache(void);
uint32_t L610_GetCachedInfoAgeMs(void);
void L610_PrintInfo(const L610_Info_t *info);
L610_Status_t L610_SelfTest(void);

#endif
