#include "perf.h"
#include "main.h"
#include "bench_time.h"
extern uint8_t _estack;
extern uint32_t _Min_Stack_Size;
void bench_clock_init(void) {}
uint32_t bench_us(void) { return bench_tick_us(HAL_GetTick()); }
uint32_t bench_millis(void) { return HAL_GetTick(); }
void bench_timing_add(BenchTiming *s, uint32_t us)
{
    if (!s->count || us<s->minimum) s->minimum=us;
    if (us>s->maximum) s->maximum=us;
    s->total+=us;++s->count;
}
void bench_stack_paint(void)
{
    uint32_t *p=(uint32_t *)((uintptr_t)&_estack-(uintptr_t)&_Min_Stack_Size);
    uint32_t *stop=(uint32_t *)__get_MSP()-32;
    while (p<stop) *p++=0xB1E3CA17U;
}
uint32_t bench_stack_used(void)
{
    uint32_t *p=(uint32_t *)((uintptr_t)&_estack-(uintptr_t)&_Min_Stack_Size);
    while ((uintptr_t)p<(uintptr_t)&_estack && *p==0xB1E3CA17U) ++p;
    return (uint32_t)((uintptr_t)&_estack-(uintptr_t)p);
}
