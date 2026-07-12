// exception thrown across a function boundary, with an RAII destructor run
// during unwinding -- exercises the OS x64 unwinder walking an intermediate
// .pdata frame and the personality invoking a cleanup landing pad.
static int g_cleanup = 0;

struct Guard
{
  ~Guard() { g_cleanup = 10; }
};

__attribute__((noinline)) static void thrower()
{
  Guard guard; // must be destroyed while the exception unwinds through thrower()
  throw 7;
}

extern "C" int entry()
{
  try
  {
    thrower();
    return -1;
  }
  catch(int x)
  {
    return x + g_cleanup; // 7 + 10 == 17 iff unwind + cleanup both ran
  }
}
