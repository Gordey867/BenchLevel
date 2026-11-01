#ifndef BENCH_RX_RECOVERY_H
#define BENCH_RX_RECOVERY_H
#include <stdbool.h>
#include <stdint.h>
static inline bool bench_rx_ring_full(uint16_t head, uint16_t tail)
{ return ((head + 1U) & 255U) == tail; }
typedef struct {
    uint32_t attempts, successes, failures, bytes, last_status, last_state, abort_fail;
} BenchRxDiag;
static inline void bench_rx_start(bool (*arm)(void *), void *ctx, volatile bool *pending)
{ *pending = !arm(ctx); }
static inline bool bench_rx_retry_due(uint32_t now, uint32_t last)
{ return (uint32_t)(now-last)>=100U; }
static inline void bench_rx_record_arm(volatile BenchRxDiag *d, bool ok, uint32_t status, uint32_t state)
{
    ++d->attempts; if (ok) ++d->successes; else ++d->failures;
    d->last_status=status; d->last_state=state;
}
/* A failed callback rearm invalidates the current command and schedules retry. */
static inline void bench_rx_rearm_failed(volatile bool *poison, volatile bool *rearm)
{ *poison = true; *rearm = true; }
#endif
