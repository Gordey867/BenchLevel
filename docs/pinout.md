# Розведення BenchLevel

Джерела: [DS13867 Rev 4, таблиці 12–15, с. 31–36](https://www.st.com/resource/en/datasheet/stm32c031c6.pdf), [UM2953 Rev 2, таблиця 11, с. 20](https://www.st.com/resource/en/user_manual/um2953-stm32-nucleo64-board-mb1717-stmicroelectronics.pdf), [RM0490](https://www.st.com/resource/en/reference_manual/rm0490-stm32c0-series-advanced-armbased-32bit-mcus-stmicroelectronics.pdf), розділи GPIO, ADC, I2C, USART.

| Сигнал | МК / режим | Роз’єм Nucleo | Призначення |
|---|---|---|---|
| SCL | PB8, I2C1 AF6, open-drain | D15, CN5.10 | MPU6050 SCL, pull-up 4,7 кОм до 3V3, LA D0 |
| SDA | PB9, I2C1 AF6, open-drain | D14, CN5.9 | MPU6050 SDA, pull-up 4,7 кОм до 3V3, LA D1 |
| UART TX | PA2, USART2 AF1 | Morpho, контакт PA2 | Monitor RX і LA D2 |
| UART RX | PA3, USART2 AF1 | Morpho, контакт PA3 | Monitor TX, введення команд не використовується |
| Потенціометр | PA0, ADC1_IN0, analog | A0, CN8.1 | SIG 0..3,3 В, ADC 12 bit |
| CAL | PB0, input pull-up | D10, CN5.3 | Кнопка на GND, active-low, LA D3 |
| LED | PA5, output | D13, CN5.6 / LD4 | Бортовий LED, LA D4 |
| ADC marker | PA4, output | A2, CN8.3 | Кожний фронт означає одне завершення ADC, LA D5 |
| Bus marker | PA6, output | D12, CN5.5 | Кожний фронт позначає початок операції bus, LA D6 |
| VCC / GND | 3V3 / GND | CN6.4 / GND | Спільна земля; AD0 MPU6050 на GND |

USART2 PA2/PA3 не є Arduino D1/D0: останні тут PB6/PB7 USART1. Альтернативний I2C1 на PB6/PB7 AF6 можливий за DS13867, але він не вибраний, щоб уникнути конфлікту з Arduino UART. Заміна ADC на PA1 вимагає зміни каналу та схеми. PC13 не є CAL.

Період I2C SCL у записі 10 мкс, USART2 115200 8N1, ADC single conversion кожні 50 мс. Файл firmware.ioc, board.h та diagram.json перевіряє tools/check_contracts.py. Регенерацію CubeMX не виконано; збережено HAL і ціль STM32C031C6.
