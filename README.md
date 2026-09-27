# BenchLevel

Вирівнювання платформи.

Гордєєв Ілля Євгенович, ІО-35.

Компоненти: MPU6050 I2C, потенціометр ADC, кнопка GPIO.

Середовище: Ubuntu 24.04, Docker, STM32 HAL, Wokwi. Платформа Nucleo-C031C6.

Індикатор відхилення від каліброваної площини. ЛР1 перевіряє UART/LED, ADC init і доступність I²C. ЛР2 додає калібрування, вимірювання й обробку помилок.

`make build` збирає прошивку. Схема `diagram.json`, налаштування Wokwi `wokwi.toml`.

У VS Code відкрийте цю папку, F1 → Wokwi: Start Simulator. UART 115200 8N1 відкривається всередині симулятора. `make flash` і `make monitor` показують цю команду для форми B.

[PRD](docs/PRD.md) · [Компоненти](docs/hardware-components.md) · [Звіт ЛР1](docs/report.md) · [Перевірки](docs/verification/lab1)

Cortex-M0+ працює без FPU. Параметри Cortex-M4 із прикладу методички для цієї плати непридатні. Симуляція не підтверджує похибку фізичного приладу.
