// static init that calls a host (process) function
extern "C" void* malloc(unsigned long long);
extern "C" void free(void*);
static int g = 0;
struct Init { Init() { void* p = malloc(16); g = p ? 1 : 0; free(p); } };
static Init theInit;
extern "C" int entry() { return g; }
