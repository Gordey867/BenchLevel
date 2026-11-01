#ifndef BENCH_TIME_H
#define BENCH_TIME_H
#include <stdint.h>
#include <stdbool.h>
/* 1 ms resolution. Unsigned subtraction handles either 32-bit wrap. */
static inline uint32_t bench_tick_us(uint32_t ms) { return ms * UINT32_C(1000); }
static inline bool bench_dispatch_due(uint32_t now, bool *started, uint32_t *last, uint32_t *missed)
{
    if (*started && now == *last) return false;
    if (*started && (uint32_t)(now - *last) > 1U) *missed += (uint32_t)(now - *last) - 1U;
    *started = true; *last = now; return true;
}
#endif
