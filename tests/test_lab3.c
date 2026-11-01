#include "level.h"
#include "calibration_store.h"
#include "console.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "format.h"
#include "clock_track.h"

typedef struct { uint8_t bytes[4096];int cut;unsigned operations, erases;bool torn; } FlashModel;
static FlashModel medium;
static void read_model(void *ctx,uint32_t off,void *out,uint32_t n)
{
    FlashModel *m=ctx;assert(off+n<=sizeof(m->bytes));memcpy(out,m->bytes+off,n);
}
static bool erase_model(void *ctx,unsigned page)
{
    FlashModel *m=ctx;assert(page<2);++m->operations;
    if (m->cut==0) { memset(m->bytes+page*2048,255,m->torn?1024:0);return false; }
    if (m->cut>0) --m->cut;
    memset(m->bytes+page*2048,255,2048);++m->erases;return true;
}
static bool program_model(void *ctx,uint32_t off,const uint8_t value[8])
{
    FlashModel *m=ctx;assert(off%8==0 && off+8<=4096);++m->operations;
    unsigned n=m->cut==0?(m->torn?4U:0U):8U;
    for (unsigned i=0;i<n;i++) { assert((m->bytes[off+i]&value[i])==value[i]);m->bytes[off+i]&=value[i]; }
    if (m->cut==0) return false;
    if (m->cut>0) --m->cut;
    return true;
}
static CalibrationMedia media(void)
{
    CalibrationMedia m={&medium,read_model,erase_model,program_model};return m;
}
static void blank(CalibrationStore *s)
{
    memset(&medium,0,sizeof(medium));memset(medium.bytes,255,sizeof(medium.bytes));medium.cut=-1;
    calibration_scan(s,media());
}
static CalibrationResult result(unsigned sample)
{
    CalibrationResult r={0};r.gravity[2]=1;r.samples=sample;r.stable_ms=300;
    r.limit_x=1200;r.limit_y=3200;r.fixed_limits=true;return r;
}
static void finish(CalibrationStore *s)
{
    for (unsigned i=0;i<12 && s->phase!=CAL_IDLE && s->phase!=CAL_FAILED;i++) calibration_write_step(s);
}
static void flat_calibration(Level *l)
{
    level_init(l,0);level_tick(l,0,false,true,1365);level_calibrate(l);
    for (unsigned n=1;n<=20;n++) { level_tick(l,n*50,false,true,1365);level_sample(l,n*50,0,0,16384); }
    assert(l->calibrated && l->state==LEVEL_OK);
}
static void axes_and_stability(void)
{
    Level l;flat_calibration(&l);assert(level_limits(&l,2000,2000));
    /* 1.7 degrees on both axes, total angle exceeds 2 degrees. */
    level_sample(&l,1050,486,486,16370);
    assert(l.angle_mdeg>2000 && abs(l.axis_mdeg[0])<1800 && abs(l.axis_mdeg[1])<1800);
    assert(!l.candidate_set);
    assert(level_limits(&l,500,5000));l.filter_set=false;
    level_sample(&l,1100,486,0,16377);assert(l.candidate_set);
    for (uint32_t t=1150;t<=1350;t+=50) {
        level_tick(&l,t,false,true,1365);level_sample(&l,t,486,0,16377);
    }
    assert(l.state==LEVEL_OK);level_tick(&l,1400,false,true,1365);assert(l.state==LEVEL_TILT);
    l.filter_set=false;level_sample(&l,1400,-486,0,16377);assert(l.axis_mdeg[0]<-1600);
    assert(!level_limits(&l,499,2000));assert(!level_limits(&l,2000,5001));
}
static void rejected_sample_cancels_dwell(void)
{
    Level l;flat_calibration(&l);level_limits(&l,500,500);
    level_sample(&l,1050,1000,0,16350);assert(l.candidate_set);
    level_sample(&l,1100,32767,32767,32767);assert(!l.candidate_set);
    level_tick(&l,1350,false,true,1365);assert(l.state!=LEVEL_TILT);
}
static void crc_and_record(void)
{
    assert(calibration_crc((const uint8_t *)"123456789",9)==0xCBF43926U);
    CalibrationStore s;blank(&s);assert(!s.available);
    CalibrationResult r=result(37);assert(calibration_save(&s,&r));assert(!calibration_save(&s,&r));
    finish(&s);assert(s.phase==CAL_IDLE && s.available && s.sequence==1 && s.completed==1);
    assert(s.saved.samples==37 && s.saved.fixed_limits && s.saved.limit_x==1200);
    CalibrationStore reboot;calibration_scan(&reboot,media());assert(reboot.sequence==1);
}
static void record_input_validation(void)
{
    CalibrationStore s;blank(&s);CalibrationResult r=result(1);
    r.gravity[0]=NAN;assert(!calibration_save(&s,&r));r=result(1);
    r.gravity[2]=0.1f;assert(!calibration_save(&s,&r));r=result(1);
    r.stable_ms=299;assert(!calibration_save(&s,&r));r=result(1);
    r.limit_x=0;assert(!calibration_save(&s,&r));
    r=result(1);r.x_mdeg=1201;assert(!calibration_save(&s,&r));
}
static void torn_record_all_units(void)
{
    for (unsigned torn=0;torn<2;torn++) for (int cut=0;cut<=8;cut++) {
        CalibrationStore s;blank(&s);CalibrationResult r=result(11);calibration_save(&s,&r);finish(&s);
        medium.cut=cut;medium.torn=torn;r=result(22);assert(calibration_save(&s,&r));finish(&s);
        CalibrationStore reboot;calibration_scan(&reboot,media());
        assert(reboot.available && reboot.saved.samples==(cut<8?11U:22U));
        medium.cut=-1;r=result(33);assert(calibration_save(&reboot,&r));finish(&reboot);
        assert(reboot.saved.samples==33 && reboot.failures==0);
    }
}
static void rotation_power_cuts(void)
{
    uint8_t full[4096];CalibrationStore s;blank(&s);
    for (unsigned i=1;i<=64;i++) { CalibrationResult r=result(i);assert(calibration_save(&s,&r));finish(&s); }
    memcpy(full,medium.bytes,sizeof(full));
    for (unsigned torn=0;torn<2;torn++) for (int cut=0;cut<=9;cut++) {
        memcpy(medium.bytes,full,sizeof(full));medium.cut=cut;medium.torn=torn;
        calibration_scan(&s,media());CalibrationResult r=result(65);assert(calibration_save(&s,&r));finish(&s);
        calibration_scan(&s,media());assert(s.available && s.saved.samples==(cut<9?64U:65U));
    }
    medium.cut=-1;
}
static void failed_write_retry_keeps_previous(void)
{
    CalibrationStore s;blank(&s);CalibrationResult r=result(1);calibration_save(&s,&r);finish(&s);
    medium.cut=1;r=result(2);calibration_save(&s,&r);finish(&s);assert(s.phase==CAL_FAILED);
    medium.cut=0;r=result(3);assert(calibration_save(&s,&r));finish(&s);
    calibration_scan(&s,media());assert(s.available && s.saved.samples==1);
}
static void bit_corruption_rejected(void)
{
    CalibrationStore s;blank(&s);CalibrationResult r=result(1);calibration_save(&s,&r);finish(&s);
    uint8_t original[64];memcpy(original,medium.bytes,64);
    for (unsigned bit=0;bit<512;bit++) {
        memcpy(medium.bytes,original,64);medium.bytes[bit/8]^=(uint8_t)(1U<<(bit%8));
        calibration_scan(&s,media());assert(!s.available && s.rejected==1);
    }
}
static BenchCommand feed(Console *c,const char *text)
{
    BenchCommand cmd=CMD_NONE;while (*text) cmd=console_feed(c,*text++);return cmd;
}
static void console_boundaries(void)
{
    Console c={0};
    assert(feed(&c,"CAL\r\n")==CMD_CAL);assert(feed(&c,"STATS\n")==CMD_STATS);
    assert(feed(&c,"LIMIT 500 5000\n")==CMD_LIMIT && c.x==500 && c.y==5000);
    assert(feed(&c,"LIMIT 499 1000\n")==CMD_BAD);
    assert(feed(&c,"LIMIT 500 655360\n")==CMD_BAD);
    assert(feed(&c,"PERIOD 2\n")==CMD_PERIOD);assert(feed(&c,"PERIOD 101\n")==CMD_BAD);
    for (unsigned i=0;i<300;i++) assert(console_feed(&c,'A')==CMD_NONE);
    assert(feed(&c,"CAL\n")==CMD_BAD);assert(feed(&c,"CAL\n")==CMD_CAL);
    assert(feed(&c,"EXPORT\n")==CMD_EXPORT && feed(&c,"PROBE\n")==CMD_PROBE);
    assert(feed(&c,"LOAD\n")==CMD_LOAD && feed(&c,"SAVE\n")==CMD_SAVE);
    assert(feed(&c,"POT\n")==CMD_POT && feed(&c,"STATUS\n")==CMD_STATUS);
    assert(feed(&c,"RESET\n")==CMD_RESET);
    assert(feed(&c,"VERSION\n")==CMD_VERSION && c.last_length==7);
}
static void virtual_hour_and_wrap(void)
{
    Level l;flat_calibration(&l);level_limits(&l,1000,2000);
    for (uint32_t t=1050;t<=3601050;t+=50) {
        level_tick(&l,t,false,true,1365);level_sample(&l,t,0,0,16384);
        assert(l.state==LEVEL_OK);
    }
    assert(l.samples==72021);
    level_init(&l,0xFFFFFF00U);level_tick(&l,0xFFFFFF00U,false,true,1365);level_calibrate(&l);
    for (uint32_t i=1;i<=20;i++) { uint32_t t=0xFFFFFF00U+i*50;level_tick(&l,t,false,true,1365);level_sample(&l,t,0,0,16384); }
    assert(l.calibrated && l.state==LEVEL_OK);
}
static void bounded_integer_telemetry(void)
{
    char out[120];
    int n=bench_format(out,sizeof(out),"id=%02X x=%ld lim=%u seq=%lu state=%s",0x68U,-180000L,500U,4294967295UL,"LEVEL");
    assert(!strcmp(out,"id=68 x=-180000 lim=500 seq=4294967295 state=LEVEL") && n==(int)strlen(out));
    n=bench_format(out,5,"%lu",123456789UL);assert(n==9 && !strcmp(out,"1234"));
    assert(bench_format(NULL,0,"%u",123U)==3);
    bench_format(out,sizeof(out),"%ld",-2147483647L-1L);assert(!strcmp(out,"-2147483648"));
}
static void slow_calibration_drift_rejected(void)
{
    Level l;level_init(&l,0);level_tick(&l,0,false,true,1365);level_calibrate(&l);
    for (uint32_t i=1;i<=20;i++) {
        level_tick(&l,i*50,false,true,1365);level_sample(&l,i*50,(int16_t)(i*80),0,16384);
    }
    assert(l.calibrating && !l.calibrated && l.cal_count<20);
}
static void sampled_clock_wrap_and_hour(void)
{
    ClockTrack s={0};uint64_t elapsed=0;
    for (unsigned i=0;i<1000000;i++) {
        /* Legal, irregular polling gaps, including the largest legal gap. */
        unsigned delta=i%17==0?65535U:(i%997U)+1U;
        elapsed+=delta;clock_track_sample(&s,(uint16_t)elapsed);
        assert(s.elapsed_us==(uint32_t)elapsed);
        assert(s.elapsed_ms==(uint32_t)(elapsed/1000U));
        assert(s.remainder==elapsed%1000U);
    }
    assert(elapsed>3600000000ULL);
    ClockTrack wrap={.previous=65530,.elapsed_us=0xFFFFFFFAU,.elapsed_ms=4294967,.remainder=290};
    clock_track_sample(&wrap,14);assert(wrap.elapsed_us==14 && wrap.elapsed_ms==4294967 && wrap.remainder==310);
}
unsigned lab3_tests(void)
{
    unsigned n=0;
#define CASE(f) do { f();puts("PASS " #f);++n; } while(0)
    CASE(axes_and_stability);CASE(rejected_sample_cancels_dwell);CASE(crc_and_record);
    CASE(record_input_validation);CASE(torn_record_all_units);CASE(rotation_power_cuts);
    CASE(failed_write_retry_keeps_previous);CASE(bit_corruption_rejected);
    CASE(console_boundaries);CASE(virtual_hour_and_wrap);
    CASE(bounded_integer_telemetry);
    CASE(slow_calibration_drift_rejected);
    CASE(sampled_clock_wrap_and_hour);
    puts("HOST ONLY: virtual_hour=3600000ms samples=72021 bit_flips=512 cut_cases=38; no MCU elapsed-time claim");
    return n;
}
