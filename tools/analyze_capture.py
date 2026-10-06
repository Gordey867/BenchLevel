#!/usr/bin/env python3
"""Derive bounded views and measurements from an unchanged real Wokwi VCD."""
from pathlib import Path
from collections import Counter
import hashlib
import json
import re
import statistics

root = Path(__file__).resolve().parents[1]
source = root / "docs/captures/address-nack.vcd"
lines = source.read_text().splitlines()
assert "$timescale 1ns $end" in lines
header_end = lines.index("$enddefinitions $end")
header = lines[:header_end + 1]
channels = dict((name, symbol) for symbol, name in
                re.findall(r"\$var wire 1 (\S+) (D\d+) \$end", "\n".join(header)))
events = []
now = 0
for line in lines[header_end + 1:]:
    if line.startswith("#"):
        now = int(line[1:])
    elif line[:1] in ("0", "1", "x", "z"):
        events.append((now, line[1:], line[0]))
sha = hashlib.sha256(source.read_bytes()).hexdigest()

def window(name, start, end):
    assert end <= now
    state = {}
    for time, symbol, value in events:
        if time >= start:
            break
        state[symbol] = value
    out = header[:-1] + [f"$comment Derived from {source.name}; sha256={sha}; original window ns {start}..{end} $end", header[-1], "#0"]
    out += [state.get(symbol, "x") + symbol for symbol in channels.values()]
    last = 0
    for time, symbol, value in events:
        if start <= time <= end:
            relative = time-start
            if relative != last:
                out.append(f"#{relative}")
                last = relative
            out.append(value+symbol)
    out.append(f"#{end-start}")
    (root / "docs/captures" / name).write_text("\n".join(out)+"\n")

windows = {"normal.vcd": (490_000_000, 660_000_000),
           "nack-window.vcd": (24_525_000_000, 24_530_000_000),
           "button-calibration.vcd": (22_515_000_000, 23_620_000_000),
           "adc-period.vcd": (600_000_000, 860_000_000)}
for name, bounds in windows.items():
    window(name, *bounds)

edges = {name: [(t, v) for t, s, v in events if s == symbol] for name, symbol in channels.items()}
adc_intervals = [(b[0]-a[0])/1e6 for a, b in zip(edges["D5"], edges["D5"][1:]) if a[0] >= 600_000_000]
clock_intervals = [b[0]-a[0] for a, b in zip(edges["D0"], edges["D0"][1:])]
uart_intervals = [b[0]-a[0] for a, b in zip(edges["D2"], edges["D2"][1:])]

# Group simultaneous pin changes before detecting START/STOP and rising SCL.
groups = {}
for time, symbol, value in events:
    if symbol in (channels["D0"], channels["D1"]):
        groups.setdefault(time, {})[symbol] = value
state = {}
frames = []
frame = None
for time, changes in sorted(groups.items()):
    previous = state.copy()
    state.update(changes)
    old_scl, old_sda = previous.get(channels["D0"]), previous.get(channels["D1"])
    scl, sda = state.get(channels["D0"]), state.get(channels["D1"])
    if scl == old_scl == "1" and old_sda == "1" and sda == "0":
        frame = {"start_ns": time, "bits": [], "rises_ns": []}
    elif scl == old_scl == "1" and old_sda == "0" and sda == "1":
        if frame:
            frame["stop_ns"] = time
            frames.append(frame)
            frame = None
    elif old_scl == "0" and scl == "1" and frame:
        assert sda in ("0", "1")
        frame["bits"].append(int(sda))
        frame["rises_ns"].append(time)

for frame in frames:
    bits = frame["bits"]
    frame["bytes"] = [sum(bits[j+i] << (7-i) for i in range(8)) for j in range(0, len(bits)-8, 9)]
    frame["ack_bits"] = [bits[j+8] for j in range(0, len(bits)-8, 9)]
normal = next(f for f in frames if len(f["bytes"]) == 15 and f["bytes"][0] == 0xD1)
nack = next(f for f in frames if f["bytes"] == [0xD2] and f["ack_bits"] == [1])
vector = normal["bytes"][1:]
assert vector == [0, 0, 0, 0, 64, 0, 239, 92, 0, 0, 0, 0, 0, 0]
report = {
    "source": source.name, "source_sha256": sha,
    "vcd_time_step_ns": 1,
    "sr_downsample": 100, "sr_samplerate_hz": 10_000_000,
    "windows_original_ns": windows,
    "scl_half_period_most_common_ns": Counter(clock_intervals).most_common(3),
    "uart_edge_intervals_most_common_ns": Counter(uart_intervals).most_common(5),
    "adc_period_ms": {"min": min(adc_intervals), "median": statistics.median(adc_intervals), "max": max(adc_intervals)},
    "normal": normal, "address_nack": nack,
    "captured_vector": vector,
}
(root / "docs/captures/measurements.json").write_text(json.dumps(report, indent=2)+"\n")
fixture = "/* Real bus vector. Source address-nack.vcd sha256="+sha+"\n   First 14-byte read after 0x3B at ns "+str(normal["start_ns"])+". */\n"
fixture += "static const unsigned char captured_flat[14] = {"+", ".join(f"0x{x:02X}" for x in vector)+"};\n"
(root / "tests/captured_vector.h").write_text(fixture)
print("PASS raw edge decoding agrees with sigrok: D1/ACK, 14 data bytes, final master NACK; D2/address NACK")
print(json.dumps({"source_sha256": sha, "adc_period_ms": report["adc_period_ms"],
                  "normal_start_ns": normal["start_ns"], "normal_duration_us": (normal["stop_ns"]-normal["start_ns"])/1000,
                  "nack_start_ns": nack["start_ns"], "nack_duration_us": (nack["stop_ns"]-nack["start_ns"])/1000}, indent=2))
