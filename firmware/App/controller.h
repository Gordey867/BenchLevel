#ifndef BENCH_CONTROLLER_H
#define BENCH_CONTROLLER_H
#include "level.h"
#include "mpu6050.h"
typedef struct {
    Mpu6050 mpu;
    Level level;
    uint32_t last_request, failed_since, errors;
    bool failure_seen;
    char pending_command;
} Controller;
void controller_init(Controller *c, Bus bus, uint32_t now);
void controller_step(Controller *c, uint32_t now, bool pressed, bool adc_ok, uint16_t adc, char command);
#endif
