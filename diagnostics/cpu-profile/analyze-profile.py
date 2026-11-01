#!/usr/bin/env python3
"""Unions of explicitly marked regions; not whole-CPU utilisation."""
import argparse, hashlib, json
from pathlib import Path

p=argparse.ArgumentParser();p.add_argument('vcd',type=Path)
p.add_argument('--start-ns',type=int,required=True);p.add_argument('--end-ns',type=int,required=True)
p.add_argument('--output',type=Path,required=True);a=p.parse_args()
assert a.end_ns-a.start_ns>=10_000_000_000
names={'!':'uart_irq','"':'systick_irq','&':'i2c_irq','(':'selected_foreground'}
state={x:0 for x in names}; spans={x:[] for x in names}; on={x:None for x in names}
t=0; last=0; timescale_ok=False
with a.vcd.open() as f:
    for line in f:
        v=line.strip()
        if v == '$timescale 1ns $end': timescale_ok=True
        if v.startswith('#') and v[1:].isdigit(): t=int(v[1:]);last=max(last,t)
        elif len(v)==2 and v[0] in '01' and v[1] in names:
            k=v[1]; n=int(v[0]);old=state[k]
            if n and not old: on[k]=t
            elif old and not n and on[k] is not None:
                lo=max(on[k],a.start_ns); hi=min(t,a.end_ns)
                if hi>lo: spans[k].append((lo,hi))
                on[k]=None
            state[k]=n
assert timescale_ok, 'Expected explicit 1 ns VCD timescale'
assert last>=a.end_ns, 'VCD ends before requested window'
for k in names:
    if on[k] is not None:
        lo=max(on[k],a.start_ns)
        if a.end_ns>lo: spans[k].append((lo,a.end_ns))
def union(parts):
    merged=[]
    for x,y in sorted(parts):
        if merged and x<=merged[-1][1]: merged[-1]=(merged[-1][0],max(y,merged[-1][1]))
        else: merged.append((x,y))
    return sum(y-x for x,y in merged)
window=a.end_ns-a.start_ns
stats={names[k]:{'pulses':len(v),'high_ns':union(v),'window_percent':100*union(v)/window}
       for k,v in spans.items()}
irq=[z for k in ('!','"','&') for z in spans[k]]
everything=irq+spans['(']
result={'vcd_sha256':hashlib.sha256(a.vcd.read_bytes()).hexdigest(),
        'start_ns':a.start_ns,'end_ns':a.end_ns,'channels':stats,
        'irq_union_ns':union(irq),'marked_union_ns':union(everything),
        'marked_union_percent':100*union(everything)/window,
        'foreground_excluding_marked_irq_ns':union(everything)-union(irq),
        'foreground_excluding_marked_irq_percent':100*(union(everything)-union(irq))/window,
        'note':'Instrumented selected active branches and IRQs, overlap union; excludes unmarked work and empty polls; includes marker overhead. Diagnostic ELF only; not total CPU or S2.'}
a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,indent=2))
