#ifndef BENCH_CALIBRATION_STORE_H
#define BENCH_CALIBRATION_STORE_H
#include <stdbool.h>
#include <stdint.h>
#define CAL_PAGE_BYTES 2048U
#define CAL_RECORD_BYTES 64U
#define CAL_SLOTS 32U
/* Offsets are relative to two dedicated flash pages, never program space. */
typedef struct {
    void *context;
    void (*read)(void *, uint32_t, void *, uint32_t);
    bool (*erase)(void *, unsigned);
    bool (*program)(void *, uint32_t, const uint8_t[8]);
} CalibrationMedia;
typedef struct {
    float gravity[3];
    uint32_t samples, stable_ms;
    int32_t x_mdeg, y_mdeg;
    uint16_t limit_x, limit_y;
    bool fixed_limits;
} CalibrationResult;
typedef enum { CAL_IDLE, CAL_ERASE, CAL_PROGRAM, CAL_VERIFY, CAL_FAILED } CalibrationPhase;
typedef struct {
    CalibrationMedia media;
    CalibrationResult saved;
    CalibrationPhase phase;
    uint32_t sequence, destination, valid, rejected, failures, completed;
    uint8_t bytes[CAL_RECORD_BYTES], unit;
    bool available;
} CalibrationStore;
uint32_t calibration_crc(const uint8_t *, unsigned);
void calibration_scan(CalibrationStore *, CalibrationMedia);
bool calibration_save(CalibrationStore *, const CalibrationResult *);
void calibration_write_step(CalibrationStore *);
#endif
