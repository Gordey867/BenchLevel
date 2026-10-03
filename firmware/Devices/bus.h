#ifndef BENCH_BUS_H
#define BENCH_BUS_H
#include <stdint.h>
#include <stddef.h>
typedef enum { BUS_PENDING, BUS_OK, BUS_NACK, BUS_TIMEOUT, BUS_ERROR, BUS_BUSY } BusResult;
/* Start functions must return immediately. Buffers remain owned until completion/cancel. */
typedef struct {
    void *ctx;
    BusResult (*read)(void *, uint8_t, uint8_t, uint8_t *, size_t);
    BusResult (*write)(void *, uint8_t, uint8_t, uint8_t);
    BusResult (*poll)(void *);
    void (*cancel)(void *);
} Bus;
#endif
