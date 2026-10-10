# BenchLevel

Індикатор відхилення від каліброваної площини на Nucleo-C031C6. Гордєєв Ілля Євгенович, ІО-35.

MPU6050 через I2C вимірює гравітацію; потенціометр задає допуск 0,5..5°, кнопка CAL калібрує еталон. LED показує UNCAL, CAL, LEVEL, TILT та ERROR. HAL STM32C0 збережено.

```sh
make build
make test
make coverage
```

Потрібні Docker, make і Python 3. Відкрийте папку у VS Code, F1 → Wokwi: Start Simulator. Якщо симулятор лише відкрився, натисніть зелену кнопку Start. UART 115200 8N1 відкривається всередині Wokwi. `make flash` і `make monitor` показують команду запуску симуляції.

Для калібрування встановіть Acceleration X=0, Y=0, Z=1 g, коротко натисніть CAL і тримайте датчик нерухомо близько секунди. Зміна Acceleration X=0,6, Z=0,8 g дає близько 36,87° і TILT після фільтра та витримки. Поля Rotation задають кутову швидкість і для цієї демонстрації не потрібні. Потенціометр Home/End задає ADC 0/4095. Утримання CAL 2 с викликає запит до відсутнього 0x69, ERROR і повторний init через 1 с. Після відновлення потрібне нове калібрування. Команди UART у поточному інтерфейсі не використовуються.

[PRD](docs/PRD.md) · [Розведення](docs/pinout.md) · [Компоненти й джерела](docs/hardware-components.md) · [Архітектура](docs/architecture.md) · [Аналіз захоплень](docs/captures/analysis.md) · [Версії](docs/versions.md)

[Звіт ЛР1](docs/lab1-report.pdf) · [Звіт ЛР2](docs/lab2-report.pdf) · [Текст ЛР1](docs/report.md) · [Текст ЛР2](docs/lab2-report.md)

Локальні теги `lab1` і `lab2` фіксують завершені етапи. 19 host-тестів перевіряють App/Devices. Реальні записи Wokwi та PulseView розміщено в docs/captures і docs/screenshots. Фізичну плату не випробувано; симуляція не підтверджує точність приладу, електричні фронти або бюджет живлення.

У перевіреній VM Ubuntu 24.04 доступна пряма збірка `make -C firmware all` та `make -C tests test CC=gcc-13`. Для coverage потрібні узгоджені GCC/gcov; перевірено версії 13.3.
