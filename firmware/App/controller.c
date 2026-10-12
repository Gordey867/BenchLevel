#include "controller.h"
#include <string.h>
void controller_init(Controller *c, Bus bus, uint32_t now)
{
    memset(c, 0, sizeof(*c));
    mpu_begin(&c->mpu, bus, 0x68, now);
    level_init(&c->level, now);
    c->last_request = now;
}
void controller_step(Controller *c, uint32_t now, bool pressed, bool adc_ok, uint16_t adc, char command)
{
    level_tick(&c->level, now, pressed, adc_ok, adc);
    if (pressed && !c->previous_pressed) {
        c->pressed_since = now;
        c->long_press_fired = false;
    }
    c->previous_pressed = pressed;
    if (pressed && !c->long_press_fired && (uint32_t)(now-c->pressed_since) >= 2000U) {
        c->pending_command = 'n';
        c->long_press_fired = true;
    }
    if (command == 'i' || command == 'n') c->pending_command = command;
    if (c->pending_command && c->mpu.phase == MPU_READY) {
        mpu_begin(&c->mpu, c->mpu.bus, c->pending_command == 'n' ? 0x69 : 0x68, now);
        c->pending_command = 0;
        c->failure_seen = false;
    }
    mpu_step(&c->mpu, now);
    if (c->mpu.phase == MPU_FAILED) {
        level_fault(&c->level);
        if (!c->failure_seen) { c->failure_seen = true; c->failed_since = now; ++c->errors; }
        if ((uint32_t)(now-c->failed_since) >= 1000U) {
            mpu_begin(&c->mpu, c->mpu.bus, 0x68, now);
            c->failure_seen = false;
        }
        return;
    }
    if (c->mpu.fresh) {
        c->mpu.fresh = false;
        MpuSample *s = &c->mpu.sample;
        level_sample(&c->level, now, s->ax, s->ay, s->az);
    }
    if ((uint32_t)(now-c->last_request) >= 50U && mpu_request(&c->mpu, now)) c->last_request = now;
}
