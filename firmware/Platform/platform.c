#include "board.h"
#include "controller.h"
#include <stdio.h>
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
    HAL_GPIO_TogglePin(BUS_MARK_PORT, BUS_MARK_PIN);
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
    HAL_GPIO_TogglePin(BUS_MARK_PORT, BUS_MARK_PIN);
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
void USART2_IRQHandler(void) { HAL_UART_IRQHandler(&huart2); }
static void log_status(uint32_t now)
{
    if (tx_busy) return;
    Level *l = &controller.level;
    Mpu6050 *m = &controller.mpu;
    int length = snprintf(line, sizeof(line),
        "t=%lu selftest=%s id=%02X adc=%u adc_ok=%u tol_mdeg=%u ax=%d ay=%d az=%d norm_mg=%u angle_mdeg=%lu state=%s err=%s errors=%lu seq=%lu halerr=%lu chsel=%lu\r\n",
        (unsigned long)now, m->phase == MPU_FAILED ? "FAIL" :
        (m->id == 0x68 && l->adc_valid && m->phase >= MPU_READY ? "PASS" : "WAIT"),
        m->id, l->adc, (unsigned)l->adc_valid, l->tolerance_mdeg,
        m->sample.ax, m->sample.ay, m->sample.az, l->norm_mg, (unsigned long)l->angle_mdeg,
        level_name(l->state), mpu_error_name(m->error), (unsigned long)controller.errors,
        (unsigned long)m->sequence, (unsigned long)hi2c1.ErrorCode, (unsigned long)hadc1.Instance->CHSELR);
    if (length > 0 && (size_t)length < sizeof(line)) {
        tx_busy = true;
        if (HAL_UART_Transmit_IT(&huart2, (uint8_t *)line, (uint16_t)length) != HAL_OK) tx_busy = false;
    }
}
void bench_init(void)
{
    HAL_NVIC_SetPriority(I2C1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(I2C1_IRQn);
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}
void bench_step(void)
{
    uint32_t now = HAL_GetTick();
    /* Allow the virtual logic analyzer to attach before the first transaction. */
    if (!running) {
        if (now < 500U) return;
        Bus bus = {NULL, read_start, write_start, bus_poll, bus_cancel};
        controller_init(&controller, bus, now);
        running = true;
        last_adc_start = now-50U;
        last_log = now-500U;
    }
    if (adc_pending && __HAL_ADC_GET_FLAG(&hadc1, ADC_FLAG_EOC)) {
        adc_value = (uint16_t)HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);
        adc_pending = false;
        adc_valid = true;
        adc_completed = now;
        HAL_GPIO_TogglePin(ADC_MARK_PORT, ADC_MARK_PIN);
    }
    if (adc_pending && (uint32_t)(now-adc_since) >= 5U) {
        HAL_ADC_Stop(&hadc1); adc_pending = false; adc_valid = false;
    }
    if (adc_valid && (uint32_t)(now-adc_completed) > 100U) adc_valid = false;
    if (!adc_pending && (uint32_t)(now-last_adc_start) >= 50U) {
        last_adc_start = now;
        adc_since = now;
        adc_pending = HAL_ADC_Start(&hadc1) == HAL_OK;
        if (!adc_pending) adc_valid = false;
    }
    controller_step(&controller, now, HAL_GPIO_ReadPin(CAL_PORT, CAL_PIN) == GPIO_PIN_RESET,
                    adc_valid, adc_value, 0);
    HAL_GPIO_WritePin(Led_GPIO_Port, Led_Pin, level_led(&controller.level, now) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    if ((uint32_t)(now-last_log) >= 500U || last_state != controller.level.state ||
        (last_phase != MPU_READY && controller.mpu.phase == MPU_READY && controller.mpu.sequence == 0) ||
        (last_phase != MPU_FAILED && controller.mpu.phase == MPU_FAILED)) {
        if (!tx_busy) {
            log_status(now);
            last_log = now;
            last_state = controller.level.state;
        }
    }
    last_phase = controller.mpu.phase;
}
