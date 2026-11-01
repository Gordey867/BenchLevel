#ifndef BENCH_CALIBRATION_FLASH_H
#define BENCH_CALIBRATION_FLASH_H
#include "calibration_store.h"
CalibrationMedia bench_flash_media(void);
void bench_flash_diagnostics(uint32_t *, uint32_t *, uint32_t *);
#endif
