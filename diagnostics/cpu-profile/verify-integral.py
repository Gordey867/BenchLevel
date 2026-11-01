import json, sys
from pathlib import Path
p=Path(sys.argv[1]); lo=20_000_000_000; hi=30_000_000_000
state={k:0 for k in '!"&('}; previous=0; total=0; fg=0; irq=0; timer=0; count=0
cal_start=None; cal=[]
with p.open() as f:
    for line in f:
        s=line.strip()
        if s.startswith('#') and s[1:].isdigit():
            now=int(s[1:]); duration=max(0,min(now,hi)-max(previous,lo))
            total+=duration*int(any(state.values()))
            irq+=duration*int(any(state[k] for k in '!"&'))
            fg+=duration*int(state['('] and not any(state[k] for k in '!"&'))
            if lo<=now<hi and state['"']: timer+=duration
            previous=now
        elif len(s)==2 and s[0] in '01' and s[1] in state:
            k=s[1]; val=int(s[0]); old=state[k]
            if k=='(' and previous<100_000_000:
                if val and not old: cal_start=previous
                if not val and old and cal_start is not None: cal.append(previous-cal_start)
            if k=='"' and val and not old and lo<=previous<hi: count+=1
            state[k]=val
result={'window_ns':[lo,hi],'union_ns':total,'irq_union_ns':irq,'foreground_net_ns':fg,
        'systick_ns':timer,'systick_rises':count,'boot_marker_pulse_widths_ns':cal}
(p.parent/'independent-integral.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
