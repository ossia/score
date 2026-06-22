// MRE: JITLink COFF fails on leaderless COMDAT .xdata/.pdata (MinGW SEH unwind).
inline void boom(int x) { if (x) throw x; }   // inline -> COMDAT; throw -> .pdata/.xdata
void (*keep)(int) = &boom;                     // keep the COMDAT function alive
