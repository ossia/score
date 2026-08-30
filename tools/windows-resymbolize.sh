#!/usr/bin/env bash
# Re-symbolize an AddressSanitizer report from a Windows score build, using cdb.
#
#   tools/windows-resymbolize.sh <asan-log> [score.exe] [symbol-dir]
#
# Run it ON the Windows box, from an MSYS2 shell.
#
# Why this exists: the names AddressSanitizer prints on Windows are nearest-
# public-symbol guesses and are routinely WRONG -- a screen-output test reports
# frames in libremidi and dr_wav; a vector reallocation reports
# libremidi::reader::parse. Acting on them wastes hours.
#
# The cause is not a missing PDB. score builds Windows with -gcodeview and
# -Wl,--pdb= deliberately (cmake/ScoreCodeviewWindows.cmake -- GCC's CodeView
# writer segfaults on boost::container types, so it is gated to clang), and
# --pdb makes lld drop the COFF symbol table entirely, which is why ASan has
# only PDB publics to guess from. The PDB is complete; llvm-symbolizer simply
# cannot read it. cdb can.
#
# cdb stops at the loader breakpoint, so the module is mapped and nothing has
# run: fast, and safe even on a GUI application.

set -u

LOG="${1:?usage: windows-resymbolize.sh <asan-log> [score.exe] [symbol-dir]}"
EXE="${2:-/d/tmp/score-stack-build/score.exe}"
SYMDIR="${3:-$(dirname "$EXE")}"

CDB="/c/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe"
[ -x "$CDB" ] || { echo "cdb not found at $CDB -- install the Windows SDK Debugging Tools" >&2; exit 1; }
[ -f "$EXE" ] || { echo "no such binary: $EXE" >&2; exit 1; }

# The image base clang/lld gives the binary. ASan prints absolute addresses;
# cdb's `ln <module>+<rva>` wants the offset.
BASE=$((0x140000000))

# cdb names the module after the file, so this must follow $EXE and not be
# assumed to be "score": the gfx suite's binaries are test_gfx_<name>.exe, and
# an `ln score+<rva>` against one of those resolves nothing and says only
# "Couldn't resolve error at ...", which reads like a missing PDB.
MODULE="$(basename "$EXE")"; MODULE="${MODULE%.exe}"

SCRIPT="$(mktemp)"
trap 'rm -f "$SCRIPT"' EXIT

# .lines -e first, or ln reports the function with no file:line.
printf '.lines -e\n' > "$SCRIPT"

n=0
while read -r addr; do
  rva=$(( addr - BASE ))
  [ "$rva" -gt 0 ] || continue
  printf '.echo @@%x@@\nln %s+0x%x\n' "$rva" "$MODULE" "$rva" >> "$SCRIPT"
  n=$((n + 1))
done < <(grep -aoE '\+0x1[0-9a-f]{8}' "$LOG" | sed 's/^+//' | sort -u | while read -r h; do printf '%d\n' "$h"; done)

printf 'q\n' >> "$SCRIPT"
echo "resolving $n unique addresses..." >&2

export _NT_SYMBOL_PATH="$(cygpath -w "$SYMDIR" 2>/dev/null || echo "$SYMDIR")"

# -cf, NOT -c: -c truncates a long script (measured: ~112 of 142 addresses).
"$CDB" -cf "$SCRIPT" "$(cygpath -w "$EXE" 2>/dev/null || echo "$EXE")"
