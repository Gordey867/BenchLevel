#include "level.h"
#include <math.h>
#include <string.h>
static float norm(const float v[3]) { return sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]); }
void level_init(Level *l, uint32_t now)
{
    memset(l, 0, sizeof(*l));
    l->state = LEVEL_UNCAL;
    l->last_good = now;
    l->tolerance_mdeg = 500;
    l->axis_limit[0] = l->axis_limit[1] = 500;
}
void level_fault(Level *l)
{
    l->state = LEVEL_ERROR;
    l->calibrated = false;
    l->calibrating = false;
    l->cal_count = 0;
    l->candidate_set = false;
    l->filter_set = false;
}
void level_calibrate(Level *l)
{
    if (l->state == LEVEL_ERROR || !l->adc_valid) return;
    l->calibrating = true;
    l->cal_count = 0;
    memset(l->sum, 0, sizeof(l->sum));
    l->state = LEVEL_CAL;
    l->candidate_set = false;
}
bool level_limits(Level *l, uint16_t x, uint16_t y)
{
    if (x < 500 || x > 5000 || y < 500 || y > 5000) return false;
    l->axis_limit[0] = x; l->axis_limit[1] = y;
    l->fixed_limits = true;
    l->candidate_set = false;
    return true;
}
void level_tick(Level *l, uint32_t now, bool pressed, bool adc_valid, uint16_t adc)
{
    l->adc_valid = adc_valid;
    if (adc_valid) {
        l->adc = adc > 4095 ? 4095 : adc;
        l->tolerance_mdeg = (uint16_t)(500U + (uint32_t)l->adc * 4500U / 4095U);
    }
    if (!l->fixed_limits) l->axis_limit[0] = l->axis_limit[1] = l->tolerance_mdeg;
    if (pressed != l->raw_button) { l->raw_button = pressed; l->button_since = now; }
    if (l->stable_button != l->raw_button && (uint32_t)(now-l->button_since) >= 30U) {
        l->stable_button = l->raw_button;
        if (l->stable_button && l->state != LEVEL_ERROR && adc_valid) {
            level_calibrate(l);
        }
    }
    if (!adc_valid || (uint32_t)(now-l->last_good) > 250U) { level_fault(l); return; }
    if (l->candidate_set && (uint32_t)(now-l->candidate_since) >= 300U) {
        l->state = l->candidate;
        l->candidate_set = false;
    }
}
void level_sample(Level *l, uint32_t now, int16_t ax, int16_t ay, int16_t az)
{
    float v[3] = {ax/16384.0f, ay/16384.0f, az/16384.0f};
    float n = norm(v);
    l->norm_mg = (uint16_t)(n * 1000.0f + 0.5f);
    if (n < 0.85f || n > 1.15f || !l->adc_valid) {
        l->candidate_set = false;
        if (l->calibrating) { l->cal_count = 0; memset(l->sum, 0, sizeof(l->sum)); }
        return;
    }
    l->last_good = now;
    ++l->samples;
    if (l->state == LEVEL_ERROR) l->state = LEVEL_UNCAL;
    for (unsigned i=0; i<3; ++i) v[i] /= n;
    if (l->calibrating) {
        float delta[3];
        for (unsigned i=0; i<3; ++i) delta[i] = v[i]-l->previous[i];
        if (l->cal_count && norm(delta) > 0.02f) {
            l->cal_count = 0;
            memset(l->sum, 0, sizeof(l->sum));
        }
        /* Keep the first vector of this window as the stability anchor.
           Pairwise small steps must not accumulate into a moving calibration. */
        for (unsigned i=0; i<3; ++i) {
            l->sum[i] += v[i];
            if (!l->cal_count) l->previous[i] = v[i];
        }
        if (++l->cal_count >= 20U) {
            float sn = norm(l->sum);
            for (unsigned i=0; i<3; ++i) l->reference[i] = l->sum[i]/sn;
            l->calibrated = true;
            l->calibrating = false;
            l->filter_set = false;
            l->state = LEVEL_OK;
            l->angle_mdeg = 0;
            l->axis_mdeg[0] = l->axis_mdeg[1] = 0;
        }
        return;
    }
    if (!l->calibrated) return;
    for (unsigned i=0; i<3; ++i)
        l->filtered[i] = l->filter_set ? l->filtered[i] + 0.25f*(v[i]-l->filtered[i]) : v[i];
    l->filter_set = true;
    float fn = norm(l->filtered);
    /* Near-opposite vectors may cancel during filtering. Restart rather than divide by zero. */
    if (fn < 0.01f) { memcpy(l->filtered, v, sizeof(v)); fn = 1.0f; }
    float dot = 0;
    for (unsigned i=0; i<3; ++i) dot += l->reference[i]*l->filtered[i]/fn;
    if (dot > 1.0f) dot = 1.0f;
    if (dot < -1.0f) dot = -1.0f;
    l->angle_mdeg = (uint32_t)(atan2f(sqrtf(1.0f-dot*dot),dot)*57295.7795f + 0.5f);
    /* Independent body X/Y inclinations in [-90,+90], relative to gravity.
       Yaw is unobservable. The dot-product guard rejects opposite hemispheres. */
    for (unsigned i=0; i<2; ++i) {
        unsigned j = 1U-i;
        float current = atan2f(l->filtered[i], sqrtf(l->filtered[j]*l->filtered[j]+l->filtered[2]*l->filtered[2]));
        float base = atan2f(l->reference[i], sqrtf(l->reference[j]*l->reference[j]+l->reference[2]*l->reference[2]));
        l->axis_mdeg[i] = (int32_t)((current-base)*57295.7795f);
    }
    /* A reversed platform must never look LEVEL solely because X and Y are zero. */
    uint32_t x = (uint32_t)(l->axis_mdeg[0]<0 ? -l->axis_mdeg[0] : l->axis_mdeg[0]);
    uint32_t y = (uint32_t)(l->axis_mdeg[1]<0 ? -l->axis_mdeg[1] : l->axis_mdeg[1]);
    LevelState next = l->state;
    if (x > l->axis_limit[0] || y > l->axis_limit[1] || dot < 0) next = LEVEL_TILT;
    else if (x+200U <= l->axis_limit[0] && y+200U <= l->axis_limit[1]) next = LEVEL_OK;
    if (next == l->state) l->candidate_set = false;
    else if (!l->candidate_set || l->candidate != next) {
        l->candidate_set = true; l->candidate = next; l->candidate_since = now;
    }
}
bool level_led(const Level *l, uint32_t now)
{
    switch (l->state) {
    case LEVEL_OK: return true;
    case LEVEL_CAL: return (now/100U)%2U;
    case LEVEL_TILT: return (now/500U)%2U;
    case LEVEL_ERROR: return (now/125U)%2U;
    default: return false;
    }
}
const char *level_name(LevelState s)
{
    static const char *const names[] = {"UNCAL", "CAL", "LEVEL", "TILT", "ERROR"};
    return s <= LEVEL_ERROR ? names[s] : "UNKNOWN";
}
