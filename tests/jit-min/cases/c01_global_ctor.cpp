// static init, no host call, no TLS
static int g = 0;
struct Init { Init() { g = 7; } };
static Init theInit;
extern "C" int entry() { return g; }
