#ifndef BENCH_BOARD_H
#define BENCH_BOARD_H
#include "main.h"
#define CAL_PORT GPIOB
#define CAL_PIN GPIO_PIN_0
#define ADC_MARK_PORT GPIOA
#define ADC_MARK_PIN GPIO_PIN_4
#define BUS_MARK_PORT GPIOA
#define BUS_MARK_PIN GPIO_PIN_6
void bench_init(void);
void bench_step(void);
#endif
