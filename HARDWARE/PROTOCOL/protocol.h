#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include "main.h"
#include <stdint.h>

typedef enum
{
    PROTOCOL_OK = 0,
    PROTOCOL_UNKNOWN_CMD,
    PROTOCOL_INVALID_PARAM,
    PROTOCOL_EXEC_ERROR
} ProtocolStatus_t;

ProtocolStatus_t Protocol_Parse(char *cmd);
void Protocol_GetStatusString(char *out_buf, uint16_t buf_size);
const char *Protocol_GetLastResponse(void);
const char *Protocol_GetLastStatusText(void);

#endif
