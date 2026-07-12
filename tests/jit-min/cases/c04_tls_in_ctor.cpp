// TLS access during static init -- the prime suspect
thread_local int t = 5;
static int g = 0;
struct Init { Init() { g = t; } };
static Init theInit;
extern "C" int entry() { return g; }
