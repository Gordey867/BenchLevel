#!/usr/bin/env python3
"""Crop a real VCD with observed initial values; never synthesize transitions."""
import argparse
import gzip
import hashlib
from pathlib import Path
import re

parser = argparse.ArgumentParser()
parser.add_argument("source", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("start_ns", type=int)
parser.add_argument("end_ns", type=int)
args = parser.parse_args()
raw = gzip.decompress(args.source.read_bytes()) if args.source.suffix == ".gz" else args.source.read_bytes()
lines = raw.decode().splitlines()
end_header = lines.index("$enddefinitions $end")
header = lines[:end_header+1]
symbols = re.findall(r"\$var wire 1 (\S+) D\d+ \$end", "\n".join(header))
state = {}
body = []
now = 0
last = None
for line in lines[end_header+1:]:
    if line.startswith("#"):
        now = int(line[1:])
    elif line[:1] in ("0", "1", "x", "z"):
        if now < args.start_ns:
            state[line[1:]] = line[0]
        elif now <= args.end_ns:
            time = now-args.start_ns
            if time != last:
                body.append(f"#{time}")
                last = time
            body.append(line)
assert now >= args.end_ns, "Requested window exceeds the real capture"
comment = f"$comment Source={args.source.name} sha256={hashlib.sha256(raw).hexdigest()} original_ns={args.start_ns}..{args.end_ns} $end"
out = header[:-1]+[comment, header[-1], "#0"]+[state.get(s,"x")+s for s in symbols]+body+[f"#{args.end_ns-args.start_ns}"]
args.output.write_text("\n".join(out)+"\n")
print(f"PASS derived window {args.output.name}; source end={now} ns")
