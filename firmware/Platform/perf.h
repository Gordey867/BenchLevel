#ifndef BENCH_PERF_H
#define BENCH_PERF_H
#include <stdint.h>
typedef struct { uint32_t count, minimum, maximum; uint64_t total; } BenchTiming;
void bench_clock_init(void);
uint32_t bench_us(void);
uint32_t bench_millis(void);
void bench_timing_add(BenchTiming *, uint32_t);
void bench_stack_paint(void);
uint32_t bench_stack_used(void);
uint32_t bench_heap_used(void);
#endif
