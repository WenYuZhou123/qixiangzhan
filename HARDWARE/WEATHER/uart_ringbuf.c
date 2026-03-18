#include "uart_ringbuf.h"

void UartRingBuf_Init(UartRingBuf_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
    memset(rb->buf, 0, sizeof(rb->buf));
}

uint16_t UartRingBuf_Available(UartRingBuf_t *rb)
{
    return (rb->head >= rb->tail) ? (rb->head - rb->tail) :
           (UART_RBUF_SIZE - rb->tail + rb->head);
}

uint8_t UartRingBuf_Push(UartRingBuf_t *rb, uint8_t data)
{
    uint16_t next = (rb->head + 1) % UART_RBUF_SIZE;
    if(next == rb->tail) return 0; // full
    rb->buf[rb->head] = data;
    rb->head = next;
    return 1;
}

uint8_t UartRingBuf_Pop(UartRingBuf_t *rb, uint8_t *data)
{
    if(rb->head == rb->tail) return 0; // empty
    *data = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) % UART_RBUF_SIZE;
    return 1;
}

uint16_t UartRingBuf_Read(UartRingBuf_t *rb, uint8_t *dst, uint16_t len)
{
    uint16_t cnt = 0;
    while(cnt < len)
    {
        if(!UartRingBuf_Pop(rb, &dst[cnt])) break;
        cnt++;
    }
    return cnt;
}

void UartRingBuf_Clear(UartRingBuf_t *rb)
{
    rb->head = rb->tail = 0;
}
