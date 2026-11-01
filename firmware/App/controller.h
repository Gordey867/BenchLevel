#ifndef BENCH_CONTROLLER_H
#define BENCH_CONTROLLER_H
#include "level.h"
#include "mpu6050.h"
typedef struct {
    Mpu6050 mpu;
    Level level;
    uint32_t last_request, failed_since, errors;
    uint32_t requests, skipped, request_late_max;
    uint16_t sample_period;
    bool failure_seen;
    char pending_command;
    uint32_t pressed_since;
    bool previous_pressed, long_press_fired;
} Controller;
void controller_init(Controller *c, Bus bus, uint32_t now);
void controller_step(Controller *c, uint32_t now, bool pressed, bool adc_ok, uint16_t adc, char command);
#endif
