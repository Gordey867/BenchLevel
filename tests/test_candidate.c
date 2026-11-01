#include "bench_time.h"
#include "rx_recovery.h"
#include "console.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

typedef struct { int calls; int gpio_low; bool succeed; } FakeRx;
static bool fake_arm(void *ctx)
{
    FakeRx *f=ctx; ++f->calls; return f->succeed;
}
int main(void)
{
    assert((uint32_t)(bench_tick_us(4294968U)-bench_tick_us(4294967U)) == 1000U);
    assert((uint32_t)(bench_tick_us(0U)-bench_tick_us(UINT32_MAX)) == 1000U);
    bool started=false; uint32_t last=0, missed=0;
    assert(bench_dispatch_due(UINT32_MAX-1U,&started,&last,&missed));
    assert(!bench_dispatch_due(UINT32_MAX-1U,&started,&last,&missed));
    assert(bench_dispatch_due(UINT32_MAX,&started,&last,&missed));
    assert(bench_dispatch_due(0,&started,&last,&missed));
    assert(bench_dispatch_due(5,&started,&last,&missed) && missed==4);
    assert(!bench_dispatch_due(5,&started,&last,&missed) && missed==4);
    assert(bench_rx_ring_full(255U,0U));
    assert(bench_rx_ring_full(4U,5U));
    assert(!bench_rx_ring_full(4U,6U));
    FakeRx fake={.gpio_low=1,.succeed=true};
    volatile bool pending=true;
    bench_rx_start(fake_arm,&fake,&pending);
    assert(fake.gpio_low && fake.calls==1 && !pending);
    fake.succeed=false; bench_rx_start(fake_arm,&fake,&pending);
    assert(fake.calls==2 && pending);
    assert(!bench_rx_retry_due(199,100) && bench_rx_retry_due(200,100));
    assert(bench_rx_retry_due(20,UINT32_MAX-80U));
    fake.succeed=true; bench_rx_start(fake_arm,&fake,&pending);
    assert(fake.calls==3 && !pending);
    volatile BenchRxDiag d={0}; bench_rx_record_arm(&d,false,2,7);
    bench_rx_record_arm(&d,true,0,8);
    assert(d.attempts==2 && d.failures==1 && d.successes==1 && d.last_state==8);
    volatile bool poison=false, rearm=false;
    bench_rx_rearm_failed(&poison,&rearm);
    assert(poison && rearm);
    Console c={0}; c.used=0; c.discard=true;
    assert(console_feed(&c,'C')==CMD_NONE);
    assert(console_feed(&c,'\n')==CMD_BAD);
    assert(console_feed(&c,'C')==CMD_NONE);
    assert(console_feed(&c,'A')==CMD_NONE);
    assert(console_feed(&c,'L')==CMD_NONE);
    assert(console_feed(&c,'\n')==CMD_CAL);
    puts("candidate tick, startup/retry, RX recovery policy and next command: OK");
}
