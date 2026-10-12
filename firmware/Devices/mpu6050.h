#ifndef BENCH_MPU6050_H
#define BENCH_MPU6050_H
#include "bus.h"
#include <stdbool.h>
typedef struct { int16_t ax, ay, az, temperature, gx, gy, gz; } MpuSample;
typedef enum { MPU_ID, MPU_POWER, MPU_SETTLE, MPU_CONFIG, MPU_GYRO,
               MPU_ACCEL, MPU_READY, MPU_SAMPLE, MPU_FAILED } MpuPhase;
typedef enum { MPU_NO_ERROR, MPU_NACK, MPU_TIMEOUT, MPU_BAD_ID, MPU_IO } MpuError;
typedef struct {
    Bus bus;
    MpuPhase phase;
    MpuError error;
    uint8_t address, id, bytes[14];
    uint32_t phase_since, transfer_since, sequence;
    bool waiting, fresh;
    MpuSample sample;
} Mpu6050;
void mpu_begin(Mpu6050 *m, Bus bus, uint8_t address, uint32_t now);
void mpu_step(Mpu6050 *m, uint32_t now);
bool mpu_request(Mpu6050 *m, uint32_t now);
MpuSample mpu_decode(const uint8_t bytes[14]);
const char *mpu_error_name(MpuError error);
#endif
