#include "mpu6050.h"
#include <string.h>
static int16_t signed_be(const uint8_t *b)
{
    uint16_t u = ((uint16_t)b[0] << 8) | b[1];
    return (int16_t)(u < 32768U ? (int32_t)u : (int32_t)u - 65536);
}
MpuSample mpu_decode(const uint8_t b[14])
{
    MpuSample s = {signed_be(b), signed_be(b+2), signed_be(b+4), signed_be(b+6),
                   signed_be(b+8), signed_be(b+10), signed_be(b+12)};
    return s;
}
void mpu_begin(Mpu6050 *m, Bus bus, uint8_t address, uint32_t now)
{
    memset(m, 0, sizeof(*m));
    m->bus = bus;
    m->address = address;
    m->phase = MPU_ID;
    m->phase_since = now;
}
static void fail(Mpu6050 *m, BusResult status)
{
    m->error = status == BUS_NACK ? MPU_NACK : status == BUS_TIMEOUT ? MPU_TIMEOUT : MPU_IO;
    m->waiting = false;
    m->phase = MPU_FAILED;
}
void mpu_step(Mpu6050 *m, uint32_t now)
{
    if (m->phase == MPU_FAILED || m->phase == MPU_READY) return;
    if (m->phase == MPU_SETTLE) {
        if ((uint32_t)(now - m->phase_since) < 100U) return;
        m->phase = MPU_CONFIG;
        m->phase_since = now;
    }
    if (m->waiting) {
        BusResult r = m->bus.poll(m->bus.ctx);
        if (r == BUS_PENDING) {
            if ((uint32_t)(now - m->transfer_since) >= 10U) {
                m->bus.cancel(m->bus.ctx);
                fail(m, BUS_TIMEOUT);
            }
            return;
        }
        m->waiting = false;
        if (r != BUS_OK) { fail(m, r); return; }
        switch (m->phase) {
        case MPU_ID:
            m->id = m->bytes[0];
            if (m->id != 0x68U) { m->error = MPU_BAD_ID; m->phase = MPU_FAILED; return; }
            m->phase = MPU_POWER;
            break;
        case MPU_POWER: m->phase = MPU_SETTLE; break;
        case MPU_CONFIG: m->phase = MPU_GYRO; break;
        case MPU_GYRO: m->phase = MPU_ACCEL; break;
        case MPU_ACCEL: m->phase = MPU_READY; break;
        case MPU_SAMPLE:
            m->sample = mpu_decode(m->bytes);
            m->fresh = true;
            ++m->sequence;
            m->phase = MPU_READY;
            break;
        default: fail(m, BUS_ERROR); return;
        }
        m->phase_since = now;
        return;
    }
    BusResult r;
    switch (m->phase) {
    case MPU_ID: r = m->bus.read(m->bus.ctx, m->address, 0x75, m->bytes, 1); break;
    case MPU_POWER: r = m->bus.write(m->bus.ctx, m->address, 0x6B, 1); break;
    case MPU_CONFIG: r = m->bus.write(m->bus.ctx, m->address, 0x1A, 3); break;
    case MPU_GYRO: r = m->bus.write(m->bus.ctx, m->address, 0x1B, 0); break;
    case MPU_ACCEL: r = m->bus.write(m->bus.ctx, m->address, 0x1C, 0); break;
    case MPU_SAMPLE: r = m->bus.read(m->bus.ctx, m->address, 0x3B, m->bytes, 14); break;
    default: return;
    }
    if (r == BUS_PENDING) { m->waiting = true; m->transfer_since = now; }
    else if (r == BUS_BUSY) {
        if ((uint32_t)(now - m->phase_since) >= 10U) {
            m->bus.cancel(m->bus.ctx);
            fail(m, BUS_TIMEOUT);
        }
    } else { fail(m, r); }
}
bool mpu_request(Mpu6050 *m, uint32_t now)
{
    if (m->phase != MPU_READY) return false;
    m->phase = MPU_SAMPLE;
    m->phase_since = now;
    return true;
}
const char *mpu_error_name(MpuError error)
{
    static const char *const names[] = {"OK", "NACK", "TIMEOUT", "BAD_ID", "IO"};
    return error <= MPU_IO ? names[error] : "UNKNOWN";
}
