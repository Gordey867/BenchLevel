#ifndef BENCH_CONSOLE_H
#define BENCH_CONSOLE_H
#include <stdint.h>
#include <stdbool.h>
typedef enum { CMD_NONE, CMD_STATUS, CMD_STATS, CMD_CAL, CMD_SAVE, CMD_LOAD, CMD_POT,
    CMD_LIMIT, CMD_PROBE, CMD_PERIOD, CMD_EXPORT, CMD_RESET, CMD_VERSION, CMD_BAD } BenchCommand;
typedef struct { char text[48];uint8_t used, last_length;bool discard;uint32_t invalid;uint16_t x,y; } Console;
BenchCommand console_feed(Console *, char);
#endif
