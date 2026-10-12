#include "controller.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "captured_vector.h"
#include "captured_tilt.h"
typedef struct {
    BusResult start_result, completion;
    uint8_t id, data[14], address, reg, value;
    size_t length;
    unsigned reads, writes, cancels;
    uint8_t config_reg[8], config_value[8];
} Mock;
static BusResult rd(void *ctx, uint8_t addr, uint8_t reg, uint8_t *b, size_t n)
{
    Mock *m = ctx;
    m->address=addr; m->reg=reg; m->length=n; ++m->reads;
    if (reg == 0x75) { assert(n==1); b[0]=m->id; }
    else { assert(reg==0x3B && n==14); memcpy(b,m->data,14); }
    return m->start_result;
}
static BusResult wr(void *ctx, uint8_t addr, uint8_t reg, uint8_t value)
{
    Mock *m=ctx;
    m->address=addr; m->reg=reg; m->value=value;
    assert(m->writes<8);
    m->config_reg[m->writes]=reg; m->config_value[m->writes]=value; ++m->writes;
    return m->start_result;
}
static BusResult poll(void *ctx) { return ((Mock *)ctx)->completion; }
static void cancel(void *ctx) { ++((Mock *)ctx)->cancels; }
static Bus mock_bus(Mock *m)
{
    memset(m,0,sizeof(*m));m->id=0x68;m->start_result=BUS_PENDING;m->completion=BUS_OK;
    m->data[4]=0x40;
    return (Bus){m,rd,wr,poll,cancel};
}
static void ready(Mpu6050 *d, Mock *m, uint32_t now)
{
    mpu_begin(d,mock_bus(m),0x68,now);
    for(unsigned t=0;t<120;t++) mpu_step(d,now+t);
    assert(d->phase==MPU_READY && d->error==MPU_NO_ERROR && d->id==0x68);
}
static void driver_order(void)
{
    Mock m;Mpu6050 d;ready(&d,&m,0);
    assert(m.reads==1 && m.writes==4);
    const uint8_t regs[]={0x6B,0x1A,0x1B,0x1C}, values[]={1,3,0,0};
    assert(!memcmp(regs,m.config_reg,4) && !memcmp(values,m.config_value,4));
    assert(mpu_request(&d,150));assert(!mpu_request(&d,150));
    mpu_step(&d,150);mpu_step(&d,151);
    assert(d.fresh && d.sequence==1 && d.sample.az==16384 && m.address==0x68);
}
static void driver_settle(void)
{
    Mock m;Mpu6050 d;mpu_begin(&d,mock_bus(&m),0x68,0);
    for(unsigned i=0;i<4;i++)mpu_step(&d,i);
    assert(d.phase==MPU_SETTLE && m.writes==1);
    mpu_step(&d,102);assert(m.writes==1);
    mpu_step(&d,103);assert(m.writes==2);
}
static void driver_faults(void)
{
    const BusResult faults[]={BUS_NACK,BUS_TIMEOUT,BUS_ERROR,BUS_BUSY};
    for(unsigned i=0;i<4;i++) {
        Mock m;Mpu6050 d;mpu_begin(&d,mock_bus(&m),0x68,0);
        m.completion=faults[i];mpu_step(&d,0);mpu_step(&d,1);
        assert(d.phase==MPU_FAILED);
        assert(d.error==(i==0?MPU_NACK:i==1?MPU_TIMEOUT:MPU_IO));
        unsigned count=m.reads;mpu_step(&d,2);assert(m.reads==count);
    }
}
static void driver_start_faults(void)
{
    const BusResult faults[]={BUS_NACK,BUS_TIMEOUT,BUS_ERROR,BUS_OK};
    for(unsigned i=0;i<4;i++) {
        Mock m;Mpu6050 d;mpu_begin(&d,mock_bus(&m),0x69,0);
        m.start_result=faults[i];mpu_step(&d,0);
        assert(d.phase==MPU_FAILED && m.address==0x69);
    }
}
static void driver_bad_id(void)
{
    Mock m;Mpu6050 d;mpu_begin(&d,mock_bus(&m),0x68,0);m.id=0;
    mpu_step(&d,0);mpu_step(&d,1);
    assert(d.error==MPU_BAD_ID && m.writes==0);
}
static void driver_timeout_wrap(void)
{
    Mock m;Mpu6050 d;uint32_t start=UINT32_MAX-5U;
    mpu_begin(&d,mock_bus(&m),0x68,start);m.completion=BUS_PENDING;
    mpu_step(&d,start);mpu_step(&d,start+9U);assert(d.phase!=MPU_FAILED);
    mpu_step(&d,start+10U);assert(d.error==MPU_TIMEOUT && m.cancels==1);
}
static void driver_busy(void)
{
    Mock m;Mpu6050 d;mpu_begin(&d,mock_bus(&m),0x68,0);m.start_result=BUS_BUSY;
    mpu_step(&d,0);mpu_step(&d,9);assert(d.phase==MPU_ID);
    mpu_step(&d,10);assert(d.error==MPU_TIMEOUT && m.cancels==1);
    mpu_begin(&d,mock_bus(&m),0x68,20);m.start_result=BUS_BUSY;mpu_step(&d,20);
    m.start_result=BUS_PENDING;mpu_step(&d,21);mpu_step(&d,22);assert(d.phase==MPU_POWER);
}
static void driver_signed_units(void)
{
    const uint8_t b[]={0x80,0,0x7F,0xFF,0xC0,0,0xFE,0xAC,0,0x83,0xFF,0x7D,0,0};
    MpuSample s=mpu_decode(b);
    assert(s.ax==INT16_MIN && s.ay==INT16_MAX && s.az==-16384 && s.temperature==-340);
    assert(s.gx==131 && s.gy==-131 && s.gz==0);
    assert(fabsf(s.az/16384.0f+1)<0.0001f);
    assert(fabsf(s.temperature/340.0f+36.53f-35.53f)<0.0001f);
}
static void calibration(Level *l,uint32_t start)
{
    level_init(l,start);level_tick(l,start,true,true,1365);
    level_tick(l,start+30,true,true,1365);assert(l->state==LEVEL_CAL);
    for(unsigned i=0;i<20;i++) {
        uint32_t now=start+30U+i*50U;
        level_tick(l,now,true,true,1365);level_sample(l,now,0,0,16384);
    }
    assert(l->calibrated && l->state==LEVEL_OK && l->tolerance_mdeg==2000);
}
static void adc_range(void)
{
    Level l;level_init(&l,0);assert(l.state==LEVEL_UNCAL && !level_led(&l,0));
    level_tick(&l,0,false,true,0);assert(l.tolerance_mdeg==500);
    level_tick(&l,1,false,true,4095);assert(l.tolerance_mdeg==5000);
    level_tick(&l,2,false,true,65535);assert(l.adc==4095);
    level_tick(&l,3,false,false,0);assert(l.state==LEVEL_ERROR);
}
static void button_bounce_hold(void)
{
    Level l;level_init(&l,0);
    level_tick(&l,0,true,true,0);level_tick(&l,15,false,true,0);
    level_tick(&l,40,false,true,0);assert(!l.calibrating);
    level_tick(&l,50,true,true,0);level_tick(&l,79,true,true,0);assert(!l.calibrating);
    level_tick(&l,80,true,true,0);assert(l.calibrating);
    level_sample(&l,80,0,0,16384);level_tick(&l,100,true,true,0);assert(l.cal_count==1);
    level_tick(&l,110,false,true,0);level_tick(&l,140,false,true,0);assert(!l.stable_button);
}
static void calibration_motion_invalid(void)
{
    Level l;level_init(&l,0);level_tick(&l,0,true,true,0);level_tick(&l,30,true,true,0);
    level_sample(&l,30,0,0,16384);level_sample(&l,80,8192,0,14189);assert(l.cal_count==1);
    level_sample(&l,130,0,0,0);assert(l.cal_count==0);
    level_sample(&l,180,0,0,32767);assert(l.cal_count==0);
    level_tick(&l,331,true,true,0);assert(l.state==LEVEL_ERROR && !l.calibrating);
}
static void angle_and_dwell(void)
{
    Level l;calibration(&l,0);
    level_tick(&l,1030,false,true,1365);level_sample(&l,1030,8192,0,14189);
    assert(l.angle_mdeg>29990 && l.angle_mdeg<30010 && l.state==LEVEL_OK);
    for(unsigned t=1080;t<=1280;t+=50){level_tick(&l,t,false,true,1365);level_sample(&l,t,8192,0,14189);}
    level_tick(&l,1329,false,true,1365);assert(l.state==LEVEL_OK);
    level_tick(&l,1330,false,true,1365);assert(l.state==LEVEL_TILT);
    l.filter_set=false;level_sample(&l,1330,16384,0,0);assert(l.angle_mdeg==90000);
}
static void hysteresis_and_cancel(void)
{
    Level l;calibration(&l,0);l.state=LEVEL_TILT;
    l.filter_set=false;level_sample(&l,1030,543,0,16375); /* 1.90 degrees stays in hysteresis */
    assert(!l.candidate_set && l.state==LEVEL_TILT);
    l.filter_set=false;level_sample(&l,1080,0,0,16384);assert(l.candidate_set);
    l.filter_set=false;level_sample(&l,1130,8192,0,14189);assert(!l.candidate_set);
    l.filter_set=false;level_sample(&l,1180,0,0,16384);
    for(unsigned t=1230;t<=1480;t+=50){level_tick(&l,t,false,true,1365);level_sample(&l,t,0,0,16384);}
    assert(l.state==LEVEL_OK);
}
static void stale_and_recover_wrap(void)
{
    Level l;uint32_t now=UINT32_MAX-100U;level_init(&l,now);
    level_tick(&l,now,false,true,1365);level_sample(&l,now,0,0,16384);
    level_tick(&l,now+250U,false,true,1365);assert(l.state==LEVEL_UNCAL);
    level_tick(&l,now+251U,false,true,1365);assert(l.state==LEVEL_ERROR);
    level_sample(&l,now+252U,0,0,16384);assert(l.state==LEVEL_UNCAL && !l.calibrated);
}
static void led_patterns(void)
{
    Level l;calibration(&l,0);assert(level_led(&l,0));
    l.state=LEVEL_CAL;assert(!level_led(&l,0) && level_led(&l,100));
    l.state=LEVEL_TILT;assert(!level_led(&l,0) && level_led(&l,500));
    level_fault(&l);assert(!level_led(&l,0) && level_led(&l,125));
    assert(!strcmp(level_name(LEVEL_ERROR),"ERROR") && !strcmp(level_name((LevelState)20),"UNKNOWN"));
    for(unsigned i=0;i<=MPU_IO;i++)assert(strcmp(mpu_error_name((MpuError)i),"UNKNOWN"));
    assert(!strcmp(mpu_error_name((MpuError)20),"UNKNOWN"));
}
static void opposite_filter(void)
{
    Level l;calibration(&l,0);l.filter_set=true;l.filtered[2]=1.0f/3.0f;
    level_sample(&l,1030,0,0,-16384);assert(l.angle_mdeg==180000);
    l.filter_set=false;level_sample(&l,1080,0,0,-16384);assert(l.angle_mdeg==180000);
}
static void controller_events(void)
{
    Mock m;Controller c;Bus bus=mock_bus(&m);controller_init(&c,bus,0);
    for(unsigned t=0;t<150;t++)controller_step(&c,t,false,true,1365,0);
    assert(c.level.state==LEVEL_UNCAL && c.mpu.sequence>0);
    /* Command arriving during a sample is retained. Concurrent CAL cannot hide a NACK. */
    mpu_request(&c.mpu,150);controller_step(&c,150,true,true,4095,'n');
    controller_step(&c,151,true,true,4095,0);controller_step(&c,152,true,true,4095,0);
    m.completion=BUS_NACK;controller_step(&c,153,true,true,4095,0);
    controller_step(&c,154,true,true,4095,0);
    assert(c.mpu.phase==MPU_FAILED && c.level.state==LEVEL_ERROR && c.errors==1);
    uint32_t failed=c.failed_since;unsigned reads=m.reads;
    controller_step(&c,failed+999,false,true,0,0);assert(m.reads==reads);
    controller_step(&c,failed+1000,false,true,0,0);assert(c.mpu.phase==MPU_ID && c.mpu.address==0x68);
    m.completion=BUS_OK;m.writes=0;
    for(unsigned t=failed+1001;t<failed+1150;t++)controller_step(&c,t,false,true,0,0);
    assert(c.level.state==LEVEL_UNCAL);
    controller_step(&c,failed+1151,false,true,0,'i');
    assert(c.pending_command=='i' || c.mpu.phase==MPU_ID);
}
static void long_press_once(void)
{
    Mock m;Controller c;controller_init(&c,mock_bus(&m),0);
    for(unsigned t=0;t<2000;t++)controller_step(&c,t,true,true,1365,0);
    assert(!c.long_press_fired);
    controller_step(&c,2000,true,true,1365,0);assert(c.long_press_fired);
    for(unsigned t=2001;t<2010;t++)controller_step(&c,t,true,true,1365,0);
    assert(m.address==0x69);
    controller_step(&c,2010,false,true,1365,0);controller_step(&c,2011,true,true,1365,0);
    assert(!c.long_press_fired && c.pressed_since==2011);
}
static void captured_bus_vector(void)
{
    Mock m;Mpu6050 d;ready(&d,&m,0);
    memcpy(m.data,captured_flat,14);
    mpu_request(&d,150);mpu_step(&d,150);mpu_step(&d,151);
    assert(d.sample.ax==0 && d.sample.ay==0 && d.sample.az==16384);
    assert(d.sample.temperature==-4260 && d.sample.gx==0 && d.sample.gy==0 && d.sample.gz==0);
    Level l;level_init(&l,150);level_tick(&l,150,false,true,1365);
    level_sample(&l,151,d.sample.ax,d.sample.ay,d.sample.az);
    assert(l.norm_mg==1000 && l.tolerance_mdeg==2000 && l.state==LEVEL_UNCAL);
    memcpy(m.data,captured_tilt,14);
    mpu_request(&d,200);mpu_step(&d,200);mpu_step(&d,201);
    assert(d.sample.ax==9830 && d.sample.az==13107 && d.sample.ay==0);
    calibration(&l,0);
    level_tick(&l,1030,false,true,1365);
    level_sample(&l,1030,d.sample.ax,d.sample.ay,d.sample.az);
    assert(l.angle_mdeg>=36868 && l.angle_mdeg<=36870 && l.norm_mg==1000);
}
int main(void)
{
    unsigned count=0;
#define RUN(name) do { name(); ++count; printf("PASS %s\n",#name); } while(0)
    RUN(driver_order);RUN(driver_settle);RUN(driver_faults);RUN(driver_start_faults);
    RUN(driver_bad_id);RUN(driver_timeout_wrap);RUN(driver_busy);RUN(driver_signed_units);
    RUN(adc_range);RUN(button_bounce_hold);RUN(calibration_motion_invalid);RUN(angle_and_dwell);
    RUN(hysteresis_and_cancel);RUN(stale_and_recover_wrap);RUN(led_patterns);RUN(opposite_filter);
    RUN(controller_events);
    RUN(long_press_once);
    RUN(captured_bus_vector);
    printf("PASS %u tests\n",count);
    return 0;
}
