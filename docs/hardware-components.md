# Компоненти та первинні джерела

## MPU6050

| Параметр | Рішення | Джерело |
|---|---|---|
| Живлення | VDD кристала 2,375..3,46 В; стенд 3,3 В. Breakout не слід автоматично вважати 5-вольтовим. | [Product Specification, §6 і §7](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Datasheet1.pdf) |
| Струм | Типово 3,8 мА для accel+gyro, DMP вимкнено, не гарантований максимум модуля. | Product Specification, §6.3 |
| I²C | Адреса 0x68 при AD0=0, 0x69 при AD0=1; до 400 кГц. Обрано близько 100 кГц. | Product Specification, §9; [Wokwi, Pin names](https://docs.wokwi.com/parts/wokwi-mpu6050) |
| ID | WHO_AM_I 0x75, очікується 0x68 незалежно від AD0. | [Register Map, регістр 117](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf) |
| Запуск | 0x6B=0x01; 0x1A=0x03; 0x1B=0; 0x1C=0. | Register Map, регістри 26, 27, 28, 107 |
| Дані | 14 байтів від 0x3B: accel XYZ, температура, gyro XYZ. Big-endian signed 16 bit. ±2 g: 16384 LSB/g; ±250 °/с: 131 LSB/(°/с). | Register Map, §4.17..4.19; Product Specification, §6.1..6.2 |

## Потенціометр, кнопка, LED

| Компонент | Рішення | Джерело |
|---|---|---|
| Потенціометр 10 кОм | VCC=3V3, GND=GND, SIG=PA0/ADC1_IN0; 12 bit, 0..4095. Струм 0,33 мА. Сигнал 0..VDDA, не 5 В. | [Wokwi potentiometer](https://docs.wokwi.com/parts/wokwi-potentiometer); [DS13867, pin definitions, ADC characteristics](https://www.st.com/resource/en/datasheet/stm32c031c6.pdf) |
| CAL | PB0 pull-up; замикання на GND, active-low; debounce 30 мс. | [Wokwi pushbutton, Pin names і bounce](https://docs.wokwi.com/parts/wokwi-pushbutton); DS13867, GPIO characteristics |
| LED | PA5, бортовий LD4 із резистором Nucleo. | [UM2953, User LEDs, таблиця 12](https://www.st.com/resource/en/user_manual/um2953-stm32-nucleo64-board-mb1717-stmicroelectronics.pdf) |

## МК, плата та живлення

STM32C031C6: Cortex-M0+ 48 МГц, 32 КіБ Flash, 12 КіБ RAM. DS13867 задає функції в LQFP48; [RM0490](https://www.st.com/resource/en/reference_manual/rm0490-stm32c0-series-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) описує периферію; UM2953, таблиці 11–12, визначає роз'єми. PA2/PA3 є USART2; Arduino D1/D0 цієї плати є PB6/PB7 USART1. Підміна пар дала б порожній UART.

Бюджет периферії: MPU6050 3,8 мА типово, потенціометр 0,33 мА, два pull-up 4,7 кОм максимум 1,40 мА при низьких лініях, кнопка менше 0,2 мА. Сума близько 5,73 мА. Проєктний резерв 20 мА не є виміряним піковим струмом. Ліміт 3V3 залежить від власного споживання Nucleo та джерела, UM2953, Power supply. На фізичному стенді потрібна перевірка мультиметром і осцилографом.

I²C працює open-drain із зовнішніми pull-up 4,7 кОм до 3V3. За наявності pull-up на модулі врахувати їх паралельне з'єднання. Ідеальні цифрові фронти Wokwi не підтверджують фізичний rise time, запас живлення чи точність ADC.

UM2953 Rev 2, §6.5.2, с. 14 обмежує сумарний струм плати та зовнішнього навантаження на 3V3 до 500 мА. Це не доступний залишок для периферії без урахування плати.
