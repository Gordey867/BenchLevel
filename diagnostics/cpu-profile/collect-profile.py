#!/usr/bin/env python3
"""Short diagnostic profile. Host stamps schedule stimuli, never measure CPU time."""
import argparse, hashlib, json, threading, time
from pathlib import Path

p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
p.add_argument('--url',default='rfc2217://127.0.0.1:43215');a=p.parse_args()
image=Path(__file__).parent/'run-profile/firmware.elf'
expected='39d8f476aba2ebc2fbcbf388c8fe616b82eec02c386b58950e62c126f0f1f7b9'
assert hashlib.sha256(image.read_bytes()).hexdigest()==expected
import serial
a.output.mkdir(parents=True,exist_ok=False)
port=serial.serial_for_url(a.url,baudrate=115200,timeout=.1)
raw=bytearray(); stop=threading.Event(); failures=[]
def reader():
    try:
        while not stop.is_set():
            data=port.read(4096)
            if data: raw.extend(data)
    except Exception as exc: failures.append(repr(exc))
t=threading.Thread(target=reader,daemon=True);t.start()
start=time.monotonic(); entries=[]; result={'completed':False,'elf_sha256':expected}
def send(cmd):
    entries.append({'host_s':time.monotonic()-start,'command':cmd})
    port.write((cmd+'\n').encode())
try:
    send('VERSION');time.sleep(2)
    if b'source=cpu-selected-v1' not in raw: raise RuntimeError('diagnostic VERSION missing')
    send('CAL');time.sleep(3);send('STATUS');time.sleep(1)
    if b'state=LEVEL' not in raw: raise RuntimeError('LEVEL not confirmed')
    for i in range(10):
        time.sleep(2)
        if failures: raise RuntimeError(failures[0])
        send('STATUS')
    time.sleep(2);result['completed']=True
except Exception as exc:
    result['error']=repr(exc)
    raise
finally:
    stop.set();t.join(timeout=2);port.close()
    (a.output/'uart.raw').write_bytes(raw)
    (a.output/'stimuli.json').write_text(json.dumps(entries,indent=2)+'\n')
    result['uart_sha256']=hashlib.sha256(raw).hexdigest()
    result['elapsed_host_s']=time.monotonic()-start
    (a.output/'collector.json').write_text(json.dumps(result,indent=2)+'\n')
