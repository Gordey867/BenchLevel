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
void level_tick(Level *l, uint32_t now, bool pressed, bool adc_valid, uint16_t adc)
{
    l->adc_valid = adc_valid;
    if (adc_valid) {
        l->adc = adc > 4095 ? 4095 : adc;
        l->tolerance_mdeg = (uint16_t)(500U + (uint32_t)l->adc * 4500U / 4095U);
    }
    if (pressed != l->raw_button) { l->raw_button = pressed; l->button_since = now; }
    if (l->stable_button != l->raw_button && (uint32_t)(now-l->button_since) >= 30U) {
        l->stable_button = l->raw_button;
        if (l->stable_button && l->state != LEVEL_ERROR && adc_valid) {
            l->calibrating = true;
            l->cal_count = 0;
            memset(l->sum, 0, sizeof(l->sum));
            l->state = LEVEL_CAL;
            l->candidate_set = false;
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
        for (unsigned i=0; i<3; ++i) { l->sum[i] += v[i]; l->previous[i] = v[i]; }
        if (++l->cal_count >= 20U) {
            float sn = norm(l->sum);
            for (unsigned i=0; i<3; ++i) l->reference[i] = l->sum[i]/sn;
            l->calibrated = true;
            l->calibrating = false;
            l->filter_set = false;
            l->state = LEVEL_OK;
            l->angle_mdeg = 0;
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
    l->angle_mdeg = (uint32_t)(acosf(dot)*57295.7795f + 0.5f);
    LevelState next = l->state;
    if (l->angle_mdeg > l->tolerance_mdeg) next = LEVEL_TILT;
    else if (l->angle_mdeg + 200U <= l->tolerance_mdeg) next = LEVEL_OK;
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
