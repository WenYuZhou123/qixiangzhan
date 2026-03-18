#ifndef __UART_RINGBUF_H
#define __UART_RINGBUF_H

#include <stdint.h>
#include <string.h>

#define UART_RBUF_SIZE 256

typedef struct
{
    uint8_t buf[UART_RBUF_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} UartRingBuf_t;

void UartRingBuf_Init(UartRingBuf_t *rb);
uint16_t UartRingBuf_Available(UartRingBuf_t *rb);
uint8_t UartRingBuf_Push(UartRingBuf_t *rb, uint8_t data);
uint8_t UartRingBuf_Pop(UartRingBuf_t *rb, uint8_t *data);
uint16_t UartRingBuf_Read(UartRingBuf_t *rb, uint8_t *dst, uint16_t len);
void UartRingBuf_Clear(UartRingBuf_t *rb);

#endif
