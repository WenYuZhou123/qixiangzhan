#ifndef __MODBUS_RTU_H
#define __MODBUS_RTU_H

#include <stdint.h>
#include "bsp_uart.h"

uint16_t Modbus_CRC16(const uint8_t *buf, uint16_t len);

uint16_t Modbus_BuildReadHoldingRegs(uint8_t slave,
                                     uint16_t reg_addr,
                                     uint16_t reg_num,
                                     uint8_t *tx_buf);

uint8_t Modbus_ParseReadHoldingRegsResp(const uint8_t *rx,
                                        uint16_t rx_len,
                                        uint8_t slave,
                                        uint16_t reg_num,
                                        uint16_t *out_regs);

#endif
