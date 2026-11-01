#!/usr/bin/env python3
"""Bind the banner to Git base and actual uncommitted firmware sources."""
import hashlib
from pathlib import Path
import subprocess
import shutil

root = Path(__file__).resolve().parents[1]
digest = hashlib.sha256()
files = []
for file in (root / "firmware").rglob("*"):
    if (file.is_file() and "build" not in file.parts and file.name != "build_id.h"
            and file.suffix in (".c", ".h", ".s", ".ld")):
        files.append(file)
files.append(root / "firmware/Makefile")
for file in sorted(files):
    digest.update(str(file.relative_to(root)).encode() + b"\0" + file.read_bytes())
if shutil.which("git"):
    git = subprocess.check_output(["git", "-C", str(root), "rev-parse", "--short=12", "HEAD"], text=True).strip()
else:
    # The course's existing build image has no Git executable.
    git_dir = root / ".git"
    head = (git_dir / "HEAD").read_text().strip()
    if head.startswith("ref: "):
        ref = head[5:]
        loose = git_dir / ref
        if loose.exists():
            head = loose.read_text().strip()
        else:
            head = next(line.split()[0] for line in (git_dir / "packed-refs").read_text().splitlines()
                        if not line.startswith(("#", "^")) and line.endswith(" " + ref))
    git = head[:12]
text = f'#define BENCH_GIT_ID "{git}-dirty"\n#define BENCH_SOURCE_ID "{digest.hexdigest()[:16]}"\n'
out = root / "firmware/Platform/build_id.h"
if not out.exists() or out.read_text() != text:
    out.write_text(text)
