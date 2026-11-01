#include "board.h"
#include "controller.h"
#include "console.h"
#include "calibration_flash.h"
#include "perf.h"
#include "bench_time.h"
#include "rx_recovery.h"
#include "build_id.h"
#include "format.h"
#include <string.h>
extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart2;
extern ADC_HandleTypeDef hadc1;
static Controller controller;
static volatile BusResult bus_result = BUS_OK;
static uint8_t tx_bytes[2], *rx_bytes;
static uint16_t rx_length, device_address;
static bool reading;
static volatile bool register_sent;
static volatile bool tx_busy;
static char line[240];
static bool running, adc_pending, adc_valid;
static uint16_t adc_value;
static uint32_t adc_since, adc_completed, last_adc_start, last_log;
static LevelState last_state = LEVEL_ERROR;
static MpuPhase last_phase = MPU_FAILED;
static uint8_t rx_byte, rx_ring[256];
static uint32_t rx_time[256];
static volatile uint16_t rx_head, rx_tail;
static volatile uint32_t rx_lost, uart_errors;
static volatile bool rx_poison;
static volatile bool rx_rearm;
static volatile uint32_t rx_last_attempt;
static volatile BenchRxDiag rx_diag;
static Console console;
static CalibrationStore store;
static BenchTiming timing[8], sample_intervals;
static const char *const timing_names[]={"control","adc","command","flash","telemetry","loop_body","loop_period","command_latency"};
static uint32_t last_dispatch, previous_loop, previous_sample, previous_sequence;
static uint32_t telemetry_lost, command_count;
static uint32_t loop_missed, command_late, adc_timeouts, adc_service_max;
static uint32_t stable_since, stats_row, command_sequence;
static int32_t sample_jitter_min, sample_jitter_max;
static bool dispatch_started, stats_pending, response_pending, stable_tracking;
static char response[80];

static BusResult hal_result(HAL_StatusTypeDef result)
{
    if (result == HAL_OK) return BUS_PENDING;
    if (result == HAL_BUSY) return BUS_BUSY;
    return (hi2c1.ErrorCode & HAL_I2C_ERROR_AF) ? BUS_NACK : BUS_ERROR;
}
static BusResult read_start(void *ctx, uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
    (void)ctx;
    if (bus_result == BUS_PENDING) return BUS_BUSY;
    tx_bytes[0] = reg;
    rx_bytes = data;
    rx_length = (uint16_t)len;
    device_address = (uint16_t)addr << 1;
    reading = true;
    register_sent = false;
    bus_result = hal_result(HAL_I2C_Master_Transmit_IT(&hi2c1, device_address, tx_bytes, 1));
    return bus_result;
}
static BusResult write_start(void *ctx, uint8_t addr, uint8_t reg, uint8_t data)
{
    (void)ctx;
    if (bus_result == BUS_PENDING) return BUS_BUSY;
    tx_bytes[0] = reg;
    tx_bytes[1] = data;
    reading = false;
    bus_result = hal_result(HAL_I2C_Master_Transmit_IT(&hi2c1, (uint16_t)addr << 1, tx_bytes, 2));
    return bus_result;
}
static BusResult bus_poll(void *ctx)
{
    (void)ctx;
    if (register_sent && bus_result == BUS_PENDING) {
        register_sent = false;
        bus_result = hal_result(HAL_I2C_Master_Receive_IT(&hi2c1, device_address, rx_bytes, rx_length));
    }
    return bus_result;
}
static void bus_cancel(void *ctx)
{
    (void)ctx;
    HAL_NVIC_DisableIRQ(I2C1_IRQn);
    HAL_I2C_DeInit(&hi2c1);
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) bus_result = BUS_ERROR;
    else bus_result = BUS_TIMEOUT;
    HAL_NVIC_EnableIRQ(I2C1_IRQn);
}
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *h)
{
    if (h != &hi2c1) return;
    if (reading) register_sent = true;
    else bus_result = BUS_OK;
}
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *h)
{
    if (h == &hi2c1) bus_result = BUS_OK;
}
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *h)
{
    if (h == &hi2c1) bus_result = (h->ErrorCode & HAL_I2C_ERROR_AF) ? BUS_NACK : BUS_ERROR;
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *h) { if (h == &huart2) tx_busy = false; }
void I2C1_IRQHandler(void)
{
    /* Data interrupts stop after the final byte. TC/STOP/NACK/error interrupts
       remain enabled. Vendor code is unchanged. */
    if (hi2c1.XferCount == 0U) {
        if (hi2c1.State == HAL_I2C_STATE_BUSY_TX) CLEAR_BIT(hi2c1.Instance->CR1, I2C_CR1_TXIE);
        if (hi2c1.State == HAL_I2C_STATE_BUSY_RX) CLEAR_BIT(hi2c1.Instance->CR1, I2C_CR1_RXIE);
    }
    HAL_I2C_EV_IRQHandler(&hi2c1);
    if (hi2c1.XferCount == 0U) {
        if (hi2c1.State == HAL_I2C_STATE_BUSY_TX) CLEAR_BIT(hi2c1.Instance->CR1, I2C_CR1_TXIE);
        if (hi2c1.State == HAL_I2C_STATE_BUSY_RX) CLEAR_BIT(hi2c1.Instance->CR1, I2C_CR1_RXIE);
    }
    HAL_I2C_ER_IRQHandler(&hi2c1);
}
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}
static bool uart_arm_rx(void *ctx)
{
    UART_HandleTypeDef *h=ctx;
    rx_last_attempt=HAL_GetTick();
    HAL_StatusTypeDef status=HAL_UART_Receive_IT(h,&rx_byte,1);
    bench_rx_record_arm(&rx_diag,status==HAL_OK,(uint32_t)status,(uint32_t)h->RxState);
    return status==HAL_OK;
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h)
{
    if (h!=&huart2) return;
    ++rx_diag.bytes;
    uint16_t next=(rx_head+1U)&255U;
    if (rx_poison || bench_rx_ring_full(rx_head,rx_tail)) { ++rx_lost;rx_poison=true; }
    else { rx_ring[rx_head]=rx_byte;rx_time[rx_head]=bench_us();__DMB();rx_head=next; }
    if (!uart_arm_rx(h)) bench_rx_rearm_failed(&rx_poison,&rx_rearm);
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *h)
{
    if (h!=&huart2) return;
    ++uart_errors;rx_last_attempt=HAL_GetTick();bench_rx_rearm_failed(&rx_poison,&rx_rearm);
    /* A held-low RX/BREAK must not repeatedly rearm an error interrupt.
       This build uses no UART DMA, so AbortReceive only resets RX/flags. */
    (void)HAL_UART_AbortReceive(h);
}
static void send_line(void)
{
    size_t n=strlen(line);
    if (!n || n>=sizeof(line)-1 || tx_busy) return;
    tx_busy=true;
    if (HAL_UART_Transmit_IT(&huart2,(uint8_t *)line,(uint16_t)n)!=HAL_OK) {
        tx_busy=false;++telemetry_lost;
    }
}
static void status_line(uint32_t now)
{
    Level *l=&controller.level;
    (void)bench_format(line,sizeof(line),
        "t=%lu id=%02X state=%s x=%ld y=%ld limx=%u limy=%u angle=%lu adc=%u adc_ok=%u seq=%lu err=%s errors=%lu nv=%lu phase=%u\r\n",
        (unsigned long)now,controller.mpu.id,level_name(l->state),(long)l->axis_mdeg[0],
        (long)l->axis_mdeg[1],l->axis_limit[0],l->axis_limit[1],(unsigned long)l->angle_mdeg,
        l->adc,(unsigned)l->adc_valid,(unsigned long)l->samples,mpu_error_name(controller.mpu.error),
        (unsigned long)controller.errors,(unsigned long)store.sequence,(unsigned)store.phase);
}
static void adc_step(uint32_t now)
{
    if (adc_pending && __HAL_ADC_GET_FLAG(&hadc1,ADC_FLAG_EOC)) {
        if (now-adc_since>adc_service_max) adc_service_max=now-adc_since;
        adc_value=(uint16_t)HAL_ADC_GetValue(&hadc1);HAL_ADC_Stop(&hadc1);
        adc_pending=false;adc_valid=true;adc_completed=now;
        HAL_GPIO_TogglePin(ADC_MARK_PORT,ADC_MARK_PIN);
    }
    if (adc_pending && now-adc_since>=5U) { HAL_ADC_Stop(&hadc1);adc_pending=false;adc_valid=false;++adc_timeouts; }
    if (adc_valid && now-adc_completed>100U) adc_valid=false;
    if (!adc_pending && now-last_adc_start>=50U) {
        uint32_t slots=(now-last_adc_start)/50U;last_adc_start+=slots*50U;adc_since=now;
        adc_pending=HAL_ADC_Start(&hadc1)==HAL_OK;
        if (!adc_pending) adc_valid=false;
    }
}
static bool save_level(uint32_t now)
{
    Level *l=&controller.level;
    if (!l->calibrated || l->state!=LEVEL_OK || !stable_tracking || now-stable_since<300U) return false;
    CalibrationResult r={0};memcpy(r.gravity,l->reference,sizeof(r.gravity));
    r.samples=l->samples;r.stable_ms=now-stable_since;r.x_mdeg=l->axis_mdeg[0];r.y_mdeg=l->axis_mdeg[1];
    r.limit_x=l->axis_limit[0];r.limit_y=l->axis_limit[1];r.fixed_limits=l->fixed_limits;
    return calibration_save(&store,&r);
}
static void apply_command(BenchCommand cmd, uint32_t now)
{
    bool ok=true;Level *l=&controller.level;
    switch (cmd) {
    case CMD_STATUS: status_line(now);return;
    case CMD_STATS: stats_pending=true;stats_row=0;break;
    case CMD_CAL: level_calibrate(l);ok=l->calibrating;break;
    case CMD_SAVE: ok=save_level(now);break;
    case CMD_LOAD:
        ok=store.available && store.phase==CAL_IDLE && l->state!=LEVEL_ERROR;
        if (ok) {
            memcpy(l->reference,store.saved.gravity,sizeof(l->reference));
            l->calibrated=true;l->calibrating=false;l->filter_set=false;l->candidate_set=false;
            l->state=LEVEL_UNCAL;l->fixed_limits=store.saved.fixed_limits;
            l->axis_limit[0]=store.saved.limit_x;l->axis_limit[1]=store.saved.limit_y;
        }
        break;
    case CMD_POT: l->fixed_limits=false;l->candidate_set=false;break;
    case CMD_LIMIT: ok=level_limits(l,console.x,console.y);break;
    case CMD_PROBE: controller.pending_command='n';break;
    case CMD_PERIOD: controller.sample_period=console.x;controller.last_request=now;break;
    case CMD_VERSION:
        (void)bench_format(line,sizeof(line),"VERSION git=%s source=%s\r\n",BENCH_GIT_ID,BENCH_SOURCE_ID);return;
    case CMD_BAD: {
        static const char digits[]="0123456789ABCDEF";
        unsigned n=0;memcpy(line,"BAD hex=",8);n=8;
        for (unsigned i=0;i<console.last_length && i<47;i++) {
            uint8_t value=(uint8_t)console.text[i];line[n++]=digits[value>>4];line[n++]=digits[value&15];
        }
        line[n++]='\r';line[n++]='\n';line[n]=0;return;
    }
    case CMD_RESET: {
        memset(timing,0,sizeof(timing));memset(&sample_intervals,0,sizeof(sample_intervals));
        previous_sample=0;previous_loop=0;sample_jitter_min=0;sample_jitter_max=0;
        controller.skipped=0;controller.request_late_max=0;
        loop_missed=0;command_late=0;adc_timeouts=0;adc_service_max=0;
        break;
    }
    case CMD_EXPORT: {
        uint32_t attempts,checked,bad;bench_flash_diagnostics(&attempts,&checked,&bad);
        (void)bench_format(line,sizeof(line),"NV available=%u seq=%lu valid=%lu rejected=%lu failures=%lu done=%lu stable_ms=%lu samples=%lu x=%ld y=%ld erase=%lu checked=%lu bad=%lu\r\n",
            (unsigned)store.available,(unsigned long)store.sequence,(unsigned long)store.valid,
            (unsigned long)store.rejected,(unsigned long)store.failures,(unsigned long)store.completed,
            (unsigned long)store.saved.stable_ms,(unsigned long)store.saved.samples,
            (long)store.saved.x_mdeg,(long)store.saved.y_mdeg,
            (unsigned long)attempts,(unsigned long)checked,(unsigned long)bad);return;
    }
    default: ok=false;break;
    }
    (void)bench_format(line,sizeof(line),"ACK n=%lu cmd=%u ok=%u t_us=%lu\r\n",
        (unsigned long)command_sequence,(unsigned)cmd,(unsigned)ok,(unsigned long)bench_us());
}
static void command_step(uint32_t now)
{
    if (rx_poison) {
        uint32_t mask=__get_PRIMASK();__disable_irq();
        rx_lost+=(rx_head-rx_tail)&255U;
        rx_tail=rx_head;rx_poison=false;__set_PRIMASK(mask);
        console.used=0;console.discard=true;
    }
    /* Stop after a complete command until its response owns the TX buffer. */
    if (response_pending || tx_busy || stats_pending) return;
    for (unsigned budget=0;budget<32 && rx_tail!=rx_head;budget++) {
        uint16_t tail=rx_tail;char b=(char)rx_ring[tail];uint32_t stamp=rx_time[tail];
        __DMB();rx_tail=(tail+1U)&255U;
        BenchCommand cmd=console_feed(&console,b);
        if (cmd==CMD_NONE) continue;
        ++command_count;++command_sequence;
        apply_command(cmd,now);
        (void)bench_format(response,sizeof(response),"EVT n=%lu rx_us=%lu act_us=%lu resolution_us=1000\r\n",
            (unsigned long)command_sequence,(unsigned long)stamp,(unsigned long)bench_us());
        response_pending=true;
        HAL_GPIO_TogglePin(GPIOB,GPIO_PIN_1);
        uint32_t latency=bench_us()-stamp;
        bench_timing_add(&timing[7],latency);
        if (latency>20000U) ++command_late;
        send_line();return;
    }
}
static void stats_line(uint32_t now)
{
    if (stats_row<8) {
        BenchTiming *s=&timing[stats_row];
        (void)bench_format(line,sizeof(line),"STAT name=%s n=%lu timing=unavailable resolution_us=1000\r\n",
            timing_names[stats_row],(unsigned long)s->count);
    } else if (stats_row==8) {
        (void)bench_format(line,sizeof(line),"SAMPLE n=%lu min_us=%lu avg_us=%lu max_us=%lu jitter_min=%ld jitter_max=%ld skipped=%lu late_ms=%lu resolution_us=1000\r\n",
            (unsigned long)sample_intervals.count,(unsigned long)sample_intervals.minimum,
            (unsigned long)(sample_intervals.count?sample_intervals.total/sample_intervals.count:0),
            (unsigned long)sample_intervals.maximum,(long)sample_jitter_min,(long)sample_jitter_max,
            (unsigned long)controller.skipped,(unsigned long)controller.request_late_max);
    } else if (stats_row==9) {
        (void)bench_format(line,sizeof(line),"RESOURCE t_ms=%lu load_permille=unavailable stack=%lu heap=%lu rx_lost=%lu uart_errors=%lu invalid=%lu commands=%lu tx_lost=%lu\r\n",
            (unsigned long)now,(unsigned long)bench_stack_used(),
            (unsigned long)bench_heap_used(),(unsigned long)rx_lost,(unsigned long)uart_errors,
            (unsigned long)console.invalid,(unsigned long)command_count,(unsigned long)telemetry_lost);
    } else if (stats_row==10) {
        (void)bench_format(line,sizeof(line),"RXDIAG arm=%lu ok=%lu fail=%lu bytes=%lu status=%lu state=%lu abort_fail=%lu pending=%u\r\n",
            (unsigned long)rx_diag.attempts,(unsigned long)rx_diag.successes,
            (unsigned long)rx_diag.failures,(unsigned long)rx_diag.bytes,
            (unsigned long)rx_diag.last_status,(unsigned long)rx_diag.last_state,
            (unsigned long)rx_diag.abort_fail,(unsigned)rx_rearm);
    } else {
        (void)bench_format(line,sizeof(line),"COUNTERS loop_skip=%lu command_late=%lu adc_timeouts=%lu adc_service_max_ms=%lu command_resolution_us=1000\r\n",
            (unsigned long)loop_missed,(unsigned long)command_late,(unsigned long)adc_timeouts,(unsigned long)adc_service_max);
        stats_pending=false;
    }
    ++stats_row;
}
void bench_init(void)
{
    bench_clock_init();
    GPIO_InitTypeDef p={0};p.Pin=GPIO_PIN_1;p.Mode=GPIO_MODE_OUTPUT_PP;HAL_GPIO_Init(GPIOB,&p);
    p.Pin=GPIO_PIN_7;HAL_GPIO_Init(GPIOA,&p);
    HAL_NVIC_SetPriority(I2C1_IRQn,1,0);HAL_NVIC_EnableIRQ(I2C1_IRQn);
    HAL_NVIC_SetPriority(USART2_IRQn,2,0);HAL_NVIC_EnableIRQ(USART2_IRQn);
    calibration_scan(&store,bench_flash_media());
    /* Initial arm is independent of the GPIO input-data register. */
    uint32_t mask=__get_PRIMASK();__disable_irq();
    bench_rx_start(uart_arm_rx,&huart2,&rx_rearm);
    __set_PRIMASK(mask);
}
void bench_step(void)
{
    uint32_t now=bench_millis();
    if (!bench_dispatch_due(now,&dispatch_started,&last_dispatch,&loop_missed)) return;
    uint32_t start=bench_us();
    GPIOA->BSRR=GPIO_PIN_7;
    if (previous_loop) bench_timing_add(&timing[6],start-previous_loop);
    previous_loop=start;
    if (rx_rearm && bench_rx_retry_due(now,rx_last_attempt)) {
        /* No RX DMA. Rate-limit recovery, including a held-low BREAK. */
        uint32_t mask=__get_PRIMASK();__disable_irq();
        if (rx_rearm && bench_rx_retry_due(now,rx_last_attempt)) {
            rx_last_attempt=now;
            if (HAL_UART_AbortReceive(&huart2)==HAL_OK) bench_rx_start(uart_arm_rx,&huart2,&rx_rearm);
            else ++rx_diag.abort_fail;
        }
        __set_PRIMASK(mask);
    }
    if (!running) {
        Bus bus={NULL,read_start,write_start,bus_poll,bus_cancel};controller_init(&controller,bus,now);
        running=true;last_adc_start=now-50U;last_log=now;
        (void)bench_format(line,sizeof(line),"BOOT BenchLevel git=%s source=%s clock=HAL_TICK_1ms HCLK=48000000 nv=%u\r\n",
            BENCH_GIT_ID,BENCH_SOURCE_ID,(unsigned)store.available);send_line();
    }
    uint32_t t=bench_us();adc_step(now);bench_timing_add(&timing[1],bench_us()-t);
    t=bench_us();
    controller_step(&controller,now,HAL_GPIO_ReadPin(CAL_PORT,CAL_PIN)==GPIO_PIN_RESET,adc_valid,adc_value,0);
    bench_timing_add(&timing[0],bench_us()-t);
    if (controller.level.state==LEVEL_OK && !controller.level.candidate_set) {
        if (!stable_tracking) { stable_since=now;stable_tracking=true; }
    } else stable_tracking=false;
    if (controller.level.samples!=previous_sequence) {
        uint32_t stamp=bench_us();
        if (previous_sample) {
            uint32_t dt=stamp-previous_sample;bench_timing_add(&sample_intervals,dt);
            int32_t jitter=(int32_t)dt-(int32_t)controller.sample_period*1000;
            if (jitter<sample_jitter_min) sample_jitter_min=jitter;
            if (jitter>sample_jitter_max) sample_jitter_max=jitter;
        }
        previous_sample=stamp;previous_sequence=controller.level.samples;
    }
    HAL_GPIO_WritePin(Led_GPIO_Port,Led_Pin,level_led(&controller.level,now)?GPIO_PIN_SET:GPIO_PIN_RESET);
    t=bench_us();command_step(now);bench_timing_add(&timing[2],bench_us()-t);
    if (store.phase==CAL_ERASE || store.phase==CAL_PROGRAM || store.phase==CAL_VERIFY) {
        GPIOA->BSRR=GPIO_PIN_6;
        t=bench_us();calibration_write_step(&store);bench_timing_add(&timing[3],bench_us()-t);
        GPIOA->BSRR=(uint32_t)GPIO_PIN_6<<16;
    }
    t=bench_us();
    if (!tx_busy) {
        if (response_pending) { memcpy(line,response,strlen(response)+1U);response_pending=false;send_line(); }
        else if (stats_pending) { stats_line(now);send_line(); }
        else if (now-last_log>=500U || last_state!=controller.level.state || last_phase!=controller.mpu.phase) {
            /* Phase transitions within an ordinary sample do not flood UART. */
            if (now-last_log>=500U || last_state!=controller.level.state || controller.mpu.phase==MPU_FAILED) {
                status_line(now);send_line();last_log=now;last_state=controller.level.state;
            }
        }
    }
    last_phase=controller.mpu.phase;
    bench_timing_add(&timing[4],bench_us()-t);
    uint32_t elapsed=bench_us()-start;
    bench_timing_add(&timing[5],elapsed);
    GPIOA->BSRR=(uint32_t)GPIO_PIN_7<<16;
}
