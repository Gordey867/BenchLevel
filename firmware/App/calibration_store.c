#include "calibration_store.h"
#include <string.h>
#include <math.h>
#define CAL_MAGIC 0x334C5642U
#define CAL_SEAL 0x1F3A5C79U
static uint32_t word(const uint8_t *b) { return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24; }
static void put(uint8_t *b, uint32_t v) { for (unsigned i=0;i<4;i++) b[i]=(uint8_t)(v>>(i*8)); }
uint32_t calibration_crc(const uint8_t *b, unsigned n)
{
    uint32_t crc=0xFFFFFFFFU;
    for (unsigned i=0;i<n;i++) {
        crc^=b[i];
        for (unsigned bit=0;bit<8;bit++) crc=(crc>>1)^((0U-(crc&1U))&0xEDB88320U);
    }
    return ~crc;
}
static bool sensible(const CalibrationResult *r)
{
    float square=0;
    for (unsigned i=0;i<3;i++) { if (!isfinite(r->gravity[i])) return false; square+=r->gravity[i]*r->gravity[i]; }
    return square>0.98f && square<1.02f && r->limit_x>=500 && r->limit_x<=5000 &&
        r->limit_y>=500 && r->limit_y<=5000 && r->stable_ms>=300 &&
        r->x_mdeg>=-(int32_t)r->limit_x && r->x_mdeg<=(int32_t)r->limit_x &&
        r->y_mdeg>=-(int32_t)r->limit_y && r->y_mdeg<=(int32_t)r->limit_y;
}
static void decode(CalibrationResult *r, const uint8_t *b)
{
    memset(r,0,sizeof(*r));
    memcpy(r->gravity,b+8,12);
    r->limit_x=(uint16_t)word(b+20);r->limit_y=(uint16_t)(word(b+20)>>16);
    r->samples=word(b+24);r->stable_ms=word(b+28);
    r->x_mdeg=(int32_t)word(b+32);r->y_mdeg=(int32_t)word(b+36);
    r->fixed_limits=word(b+40)==1U;
}
static bool valid(const uint8_t *b, CalibrationResult *r)
{
    if (word(b)!=CAL_MAGIC || word(b+44)!=1U || word(b+52)!=~word(b+4) ||
        word(b+56)!=CAL_SEAL || word(b+60)!=~CAL_SEAL ||
        word(b+48)!=calibration_crc(b,48)) return false;
    decode(r,b);
    return sensible(r);
}
static bool erased(const uint8_t *b)
{
    for (unsigned i=0;i<CAL_RECORD_BYTES;i++) if (b[i]!=0xFF) return false;
    return true;
}
void calibration_scan(CalibrationStore *s, CalibrationMedia media)
{
    memset(s,0,sizeof(*s));s->media=media;
    uint32_t newest_offset=0;
    for (unsigned off=0;off<2U*CAL_PAGE_BYTES;off+=CAL_RECORD_BYTES) {
        uint8_t b[CAL_RECORD_BYTES];CalibrationResult candidate;
        media.read(media.context,off,b,sizeof(b));
        if (erased(b)) continue;
        if (!valid(b,&candidate)) { ++s->rejected;continue; }
        ++s->valid;
        uint32_t seq=word(b+4);
        if (!s->available || (int32_t)(seq-s->sequence)>0) {
            s->saved=candidate;s->sequence=seq;s->available=true;newest_offset=off;
        }
    }
    unsigned page=s->available ? newest_offset/CAL_PAGE_BYTES : 0U;
    s->destination=2U*CAL_PAGE_BYTES;
    for (unsigned off=page*CAL_PAGE_BYTES;off<(page+1)*CAL_PAGE_BYTES;off+=CAL_RECORD_BYTES) {
        uint8_t b[CAL_RECORD_BYTES];media.read(media.context,off,b,sizeof(b));
        if (erased(b)) { s->destination=off;break; }
    }
    if (s->destination==2U*CAL_PAGE_BYTES) s->destination=(1U-page)*CAL_PAGE_BYTES;
}
bool calibration_save(CalibrationStore *s, const CalibrationResult *r)
{
    if (s->phase!=CAL_IDLE && s->phase!=CAL_FAILED) return false;
    if (!sensible(r)) return false;
    if (s->phase==CAL_FAILED) {
        CalibrationMedia media=s->media;uint32_t fails=s->failures, done=s->completed;
        calibration_scan(s,media);s->failures=fails;s->completed=done;
    }
    memset(s->bytes,0,sizeof(s->bytes));
    put(s->bytes,CAL_MAGIC);put(s->bytes+4,s->sequence+1U);
    memcpy(s->bytes+8,r->gravity,12);
    put(s->bytes+20,(uint32_t)r->limit_x|((uint32_t)r->limit_y<<16));
    put(s->bytes+24,r->samples);put(s->bytes+28,r->stable_ms);
    put(s->bytes+32,(uint32_t)r->x_mdeg);put(s->bytes+36,(uint32_t)r->y_mdeg);
    put(s->bytes+40,r->fixed_limits?1U:0U);put(s->bytes+44,1);
    put(s->bytes+48,calibration_crc(s->bytes,48));put(s->bytes+52,~(s->sequence+1U));
    put(s->bytes+56,CAL_SEAL);put(s->bytes+60,~CAL_SEAL);
    uint8_t prior[CAL_RECORD_BYTES];s->media.read(s->media.context,s->destination,prior,sizeof(prior));
    s->phase=erased(prior)?CAL_PROGRAM:CAL_ERASE;s->unit=0;
    return true;
}
void calibration_write_step(CalibrationStore *s)
{
    bool ok=true;
    if (s->phase==CAL_ERASE) {
        ok=s->media.erase(s->media.context,s->destination/CAL_PAGE_BYTES);
        if (ok) s->phase=CAL_PROGRAM;
    } else if (s->phase==CAL_PROGRAM) {
        ok=s->media.program(s->media.context,s->destination+s->unit*8U,s->bytes+s->unit*8U);
        if (ok && ++s->unit==8U) s->phase=CAL_VERIFY;
    } else if (s->phase==CAL_VERIFY) {
        uint8_t check[CAL_RECORD_BYTES];CalibrationResult r;
        s->media.read(s->media.context,s->destination,check,sizeof(check));
        ok=valid(check,&r) && memcmp(check,s->bytes,sizeof(check))==0;
        if (ok) {
            CalibrationMedia media=s->media;uint32_t count=s->completed,failures=s->failures;
            calibration_scan(s,media);s->completed=count+1;s->failures=failures;
        }
    }
    if (!ok) { s->phase=CAL_FAILED;++s->failures; }
}
