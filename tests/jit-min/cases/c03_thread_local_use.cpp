// TLS access, but only at call time (not in a static ctor)
thread_local int t = 5;
extern "C" int entry() { return t; }
