# Версії та відтворюваність

Фактичне виконання перевірок і захоплень 16.10.2026. Дати Git є окремими метаданими й не датують вимірювання.

| Компонент | Перевірена версія / джерело |
|---|---|
| Build OS | Ubuntu 24.04.5 LTS, Docker ARM64 |
| ARM GCC | 13.2.1, пакет 15:13.2.rel1-2 |
| Host GCC | 13.3.0 |
| gcovr | 7.0 |
| STM32C0 HAL | 1.0.1, CubeC0 FW 1.0.1 |
| CMSIS Core / Device | 5.3.0 / STM32C0 1.0.0 |
| Wokwi VS Code | 4.0.0, Nucleo-C031C6 |
| PulseView | 0.4.2 |
| Libsigrok / decoder | Локальний sigrok-cli, фактичні версії в capture-tools.txt |

Хеші 84 vendored файлів у vendor-sha256.json. ADC-модулі додані з офіційного [HAL v1.0.1](https://github.com/STMicroelectronics/stm32c0xx-hal-driver/tree/v1.0.1). Інший наявний HAL збережено. Для Cortex-M0+ використано soft-float та -Os.

Dockerfile використовує тег ubuntu:24.04 й apt без фіксації версій. Чисті повторні збірки ЛР1 в одному локальному образі дали однакові SHA-256. Це не гарантує однакові байти після майбутнього apt update або на іншій архітектурі host. Фактичні версії та логи є в verification/lab1 і verification/lab2. Віддалений CI не запускався; workflow описує build/test/coverage.

Фінальна ЛР2: text 24816, data 104, bss 2680 байтів. Flash text+data 24920/32768 байтів. Резерв RAM без стеку 9504/12288 байтів. Це не вимірювання пікового стеку. HEX SHA-256 9ff9905cc3777022d4418ad57e5283903b232a4f04c1a3402ff591481ae62f62.
