#ifndef BENCH_LEVEL_H
#define BENCH_LEVEL_H
#include <stdint.h>
#include <stdbool.h>
typedef enum { LEVEL_UNCAL, LEVEL_CAL, LEVEL_OK, LEVEL_TILT, LEVEL_ERROR } LevelState;
typedef struct {
    LevelState state, candidate;
    bool calibrated, calibrating, filter_set, raw_button, stable_button;
    bool candidate_set, adc_valid;
    uint32_t button_since, last_good, candidate_since, angle_mdeg, samples;
    uint16_t adc, tolerance_mdeg, norm_mg;
    uint8_t cal_count;
    float reference[3], filtered[3], sum[3], previous[3];
} Level;
void level_init(Level *l, uint32_t now);
void level_tick(Level *l, uint32_t now, bool pressed, bool adc_valid, uint16_t adc);
void level_sample(Level *l, uint32_t now, int16_t ax, int16_t ay, int16_t az);
void level_fault(Level *l);
bool level_led(const Level *l, uint32_t now);
const char *level_name(LevelState s);
#endif
