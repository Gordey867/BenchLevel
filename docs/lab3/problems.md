# Фактичні проблеми ЛР3

Докази нижче належать BenchLevel. Виправлення ідентифікуються файлами та source ID у build-manifest.json. Час host-тестів не є часом MCU.

| Симптом | Причина | Виправлення | Перевірка |
|---|---|---|---|
| Stamp зупинив ARM-збірку: FileNotFoundError git | Наявний Docker image не містить Git executable | stamp_build.py читає HEAD/ref/packed-refs, якщо Git відсутній | build-initial.log, успішна повторна збірка |
| 30 576 B не помістилися в code region 28 КБ | Два покоління калібрування потребують 4 КБ із тих самих 32 КБ; загальний formatter і додатковий acosf збільшили код | LTO, atan2f для загального й осьових кутів, обмежений integer formatter | build-second.log: overflow 1904 B; rx-accounting-build.log і clean-current.log: 27 240 B |
| Host-тест короткого output buffer завис | Старий EMIT не обчислював аргумент після заповнення буфера, тому не просував вказівник формату/рядка | Обчислення byte перед умовою запису | verification-fourth.log закінчується на запуску тесту; bounded_integer_telemetry і sanitizer далі проходять |
| Перевірка cut при rotation очікувала зайву erase операцію | У першій ротації друга сторінка ще порожня, стирання не потрібне | Тест спершу заповнює обидві сторінки, лише потім перериває реальне стирання альтернативної | host-tests-first.log, rotation_power_cuts у 31 тесті |
| Повільний рух дав успішне калібрування | Порівнювались сусідні вектори, маленькі кроки накопичували великий дрейф | Вікно порівнюється з першим вектором, при виході за 0,02 накопичення починається знову | regression-probe.log: до calibrated=1, після calibrated=0; slow_calibration_drift_rejected |
| Відхилений за нормою відлік не скасував 300 мс candidate | Відбір сенсорних даних і timer commit користуються спільним станом, return не очищав candidate | Недопустимий sample скасовує pending candidate; SAVE також чекає відсутності candidate | regression-probe.log: до TILT, після LEVEL; rejected_sample_cancels_dwell |

Виміряний кандидат використовує HAL tick 1 мс, не TIM3 і не DWT. Його реалізацію зафіксовано в a0cbe8f; наведені вище старі журнали є історією, а не доказом відтворення кожного сценарію на фінальній збірці. Ні Wokwi flash latency, ні відключення живлення не оголошено вже випробуваними.
