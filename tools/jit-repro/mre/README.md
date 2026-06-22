# Minimal repro: JITLink COFF fails on leaderless COMDAT .xdata/.pdata (MinGW SEH)

```cpp
// boom.cpp
inline void boom(int x) { if (x) throw x; }   // inline -> COMDAT; throw -> .pdata/.xdata
void (*keep)(int) = &boom;
```

```
clang --target=x86_64-w64-windows-gnu -c boom.cpp -o boom.o
llvm-jitlink -noexec -entry=keep boom.o
# llvm-jitlink error: Could not find symbol at given index,
#   did you add it to JITSymbolTable? index: 9
```

`-fno-exceptions` removes the failure. Reproduces on LLVM 20/21/22 and from IR (`llc boom.ll`).

## Cause
clang emits per-function SEH unwind data for the COMDAT `boom` as a *leaderless*
COMDAT `.xdata$_Z4boomi` (`IMAGE_COMDAT_SELECT_ANY`, only its `Static` section
symbol -- no external leader). `.pdata$_Z4boomi` has an `ADDR32NB` relocation to
that `.xdata` section symbol. In `COFFLinkGraphBuilder`, a `Selection: Any` COMDAT
section symbol is handled by `createCOMDATExportRequest`, which defers graph-symbol
creation until an *external leader* is seen (`exportCOMDATSymbol`). Leaderless
unwind COMDATs never get a leader, so the section symbol is left ungraphified and
the `.pdata` relocation hits `getGraphSymbol() == null`.

## Fix direction
After the symbol loop in `COFFLinkGraphBuilder::graphifySymbols`, flush any
still-pending COMDAT exports by exporting the section symbol itself.
