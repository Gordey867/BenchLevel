#include "clock_track.h"
__attribute__((noinline)) void clock_track_sample(volatile ClockTrack *s, uint16_t count)
{
    uint16_t delta=(uint16_t)(count-s->previous);
    s->previous=count;s->elapsed_us+=delta;
    uint32_t remainder=(uint32_t)s->remainder+delta;
    uint32_t ms=s->elapsed_ms;
    /* At most 66 subtractions for any legal 16-bit delta; normally 0 or 1. */
    while (remainder>=1000U) { remainder-=1000U;++ms; }
    s->remainder=(uint16_t)remainder;s->elapsed_ms=ms;
}
