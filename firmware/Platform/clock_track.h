#ifndef BENCH_CLOCK_TRACK_H
#define BENCH_CLOCK_TRACK_H
#include <stdint.h>
typedef struct {
    uint16_t previous, remainder;
    uint32_t elapsed_us, elapsed_ms;
} ClockTrack;
/* Caller serializes ISR/foreground access. Successive samples must be less
   than 65536 hardware microseconds apart. No division in this hot path. */
void clock_track_sample(volatile ClockTrack *, uint16_t);
#endif
