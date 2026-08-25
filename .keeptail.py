#!/usr/bin/env python3
"""HEAD already contains the frame-determinism block (guarded); the incoming
side re-adds an unguarded copy of it plus genuinely new registrations. Keep all
of HEAD and only the part of the incoming side that follows its duplicate."""
import subprocess, sys
FD = 'ENVIRONMENT "OSSIA_SCORE=${SCORE_ROOT_BINARY_DIR}/ossia-score")\n'
files = subprocess.run(["git","diff","--name-only","--diff-filter=U"],
                       capture_output=True,text=True).stdout.split()
for f in files:
    if not f.endswith("CMakeLists.txt"): sys.exit(f"REFUSE non-cmake {f}")
    s=open(f,encoding='utf-8').read()
    n=0
    while '<<<<<<< HEAD' in s:
        i=s.index('<<<<<<< HEAD'); j=s.index('\n=======\n',i)
        k=s.index('>>>>>>>',j); end=s.index('\n',k)+1
        ours=s[i+len('<<<<<<< HEAD\n'):j+1]
        theirs=s[j+len('\n=======\n'):k]
        if theirs.lstrip().startswith('add_test(NAME test_frame_determinism'):
            if FD not in theirs: sys.exit("frame-determinism tail marker missing")
            theirs = theirs[theirs.index(FD)+len(FD):]
        s = s[:i] + ours + theirs + s[end:]
        n+=1
    open(f,'w',encoding='utf-8').write(s)
    # balance check
    import re
    low=s.lower()
    ifs=len(re.findall(r'^\s*if\(',low,re.M)); ends=len(re.findall(r'^\s*endif\(',low,re.M))
    print(f"resolved {f} x{n} | if={ifs} endif={ends}" + ("" if ifs==ends else "  <<< IMBALANCE"))
    if ifs!=ends: sys.exit(2)
