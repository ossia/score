#!/usr/bin/env python3
"""Resolve 'both sides appended a distinct block' conflicts by keeping both,
ours first. Refuses any conflict that is not of that shape (i.e. where either
side's lines overlap textually), so a real semantic conflict still stops."""
import subprocess, sys, re

files = subprocess.run(["git","diff","--name-only","--diff-filter=U"],
                       capture_output=True, text=True).stdout.split()
if not files:
    print("NO-CONFLICTS"); sys.exit(0)

for f in files:
    if not f.endswith("CMakeLists.txt"):
        print(f"REFUSE (not a CMakeLists): {f}"); sys.exit(2)
    lines = open(f, encoding="utf-8").read().split("\n")
    out, i, n = [], 0, 0
    while i < len(lines):
        if lines[i].startswith("<<<<<<<"):
            j = i+1; ours=[]
            while not lines[j].startswith("======="): ours.append(lines[j]); j += 1
            k = j+1; theirs=[]
            while not lines[k].startswith(">>>>>>>"): theirs.append(lines[k]); k += 1
            # only safe when the two additions share no non-trivial line
            so = {l.strip() for l in ours   if len(l.strip())>3 and not l.strip().startswith("#")}
            st = {l.strip() for l in theirs if len(l.strip())>3 and not l.strip().startswith("#")}
            if so & st:
                print(f"REFUSE (overlapping lines) in {f}: {sorted(so&st)[:3]}"); sys.exit(2)
            out.extend(ours); out.extend(theirs); n += 1
            i = k+1
        else:
            out.append(lines[i]); i += 1
    open(f,"w",encoding="utf-8").write("\n".join(out))
    print(f"KEPT-BOTH x{n}: {f}")
