#!/usr/bin/env python3
"""Check layer includes and the shared wiring/configuration contract."""
from pathlib import Path
import json
import re

root = Path(__file__).resolve().parents[1]
allowed = {
    "App": {"controller.h", "level.h", "mpu6050.h", "console.h", "calibration_store.h", "string.h", "stdint.h", "stdbool.h", "math.h"},
    "Devices": {"bus.h", "mpu6050.h", "string.h", "stdint.h", "stdbool.h", "stddef.h"},
}
for layer, headers in allowed.items():
    for file in (root / "firmware" / layer).glob("*.[ch]"):
        for header in re.findall(r'#include\s*[<"]([^>"]+)', file.read_text()):
            assert header in headers, f"Forbidden dependency: {file.name} -> {header}"

ioc = dict(line.split("=", 1) for line in (root / "firmware/firmware.ioc").read_text().splitlines()
           if "=" in line and not line.startswith("#"))
for key, value in {
    "PB8.Signal": "I2C1_SCL", "PB9.Signal": "I2C1_SDA", "PA0.Signal": "ADC1_IN0",
    "PA2.Signal": "USART2_TX", "PA3.Signal": "USART2_RX", "PA5.Signal": "GPIO_Output",
    "PB0.Signal": "GPIO_Input", "PB0.GPIO_PuPd": "GPIO_PULLUP",
    "PA3.GPIO_PuPd": "GPIO_PULLUP",
    "PA4.Signal": "GPIO_Output", "PA6.Signal": "GPIO_Output",
    "ADC1.ScanConvMode": "ADC_SCAN_SEQ_FIXED", "I2C1.Timing": "0x20303E5D",
}.items():
    assert ioc.get(key) == value, f"IOC mismatch: {key}"
diagram = json.loads((root / "diagram.json").read_text())
edges = {frozenset(edge[:2]) for edge in diagram["connections"]}
for a, b in [("nucleo:PB8", "imu:SCL"), ("nucleo:PB9", "imu:SDA"),
             ("nucleo:PA0", "tolerance:SIG"), ("nucleo:PB0", "calibrate:1.l"),
             ("nucleo:PA2", "$serialMonitor:RX"), ("nucleo:PA3", "$serialMonitor:TX"),
             ("nucleo:PA6", "logic:D5"), ("nucleo:PA3", "logic:D6"),
             ("nucleo:PA7", "logic:D7"), ("nucleo:PB1", "logic:D4")]:
    assert frozenset((a, b)) in edges, f"Diagram mismatch: {a}, {b}"
board = (root / "firmware/Platform/board.h").read_text()
for text in ("CAL_PORT GPIOB", "CAL_PIN GPIO_PIN_0", "ADC_MARK_PIN GPIO_PIN_4", "BUS_MARK_PIN GPIO_PIN_6"):
    assert text in board, f"Platform mismatch: {text}"
main = (root / "firmware/Core/Src/main.c").read_text()
assert "ADC_SCAN_SEQ_FIXED" in main and "0x20303E5D" in main
print("PASS layer include allowlists, IOC, diagram and platform pin contracts")
