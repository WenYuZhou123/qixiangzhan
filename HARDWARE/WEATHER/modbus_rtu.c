#include "modbus_rtu.h"

uint16_t Modbus_CRC16(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for(uint16_t pos = 0; pos < len; pos++)
    {
        crc ^= (uint16_t)buf[pos];
        for(int i = 0; i < 8; i++)
        {
            if(crc & 0x0001)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

uint16_t Modbus_BuildReadHoldingRegs(uint8_t slave,
                                     uint16_t reg_addr,
                                     uint16_t reg_num,
                                     uint8_t *tx_buf)
{
    tx_buf[0] = slave;
    tx_buf[1] = 0x03;
    tx_buf[2] = reg_addr >> 8;
    tx_buf[3] = reg_addr & 0xFF;
    tx_buf[4] = reg_num >> 8;
    tx_buf[5] = reg_num & 0xFF;

    uint16_t crc = Modbus_CRC16(tx_buf, 6);
    tx_buf[6] = crc & 0xFF;
    tx_buf[7] = crc >> 8;
    return 8;
}

uint8_t Modbus_ParseReadHoldingRegsResp(const uint8_t *rx,
                                        uint16_t rx_len,
                                        uint8_t slave,
                                        uint16_t reg_num,
                                        uint16_t *out_regs)
{
    if(rx_len < 5) return 0;
    if(rx[0] != slave || rx[1] != 0x03) return 0;
    if(rx[2] != reg_num * 2) return 0;

    uint16_t crc_calc = Modbus_CRC16(rx, rx_len - 2);
    uint16_t crc_recv = rx[rx_len - 2] | (rx[rx_len - 1] << 8);
    if(crc_calc != crc_recv) return 0;

    for(uint16_t i = 0; i < reg_num; i++)
    {
        out_regs[i] = (rx[3 + i * 2] << 8) | rx[4 + i * 2];
    }
    return 1;
}
