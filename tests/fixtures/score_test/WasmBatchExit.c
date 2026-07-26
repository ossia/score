// WebAssembly test binaries are linked with -sEXIT_RUNTIME=1 so that returning
// from main() terminates the module and reports the exit status. This overrides
// the weak musl alias to keep emscripten's EXIT_RUNTIME=0 handling of C++ global
// destructors: they are not registered, and __funcs_on_exit has nothing to call.
#include <stdlib.h>

int __cxa_atexit(void (*func)(void*), void* arg, void* dso)
{
  (void)func;
  (void)arg;
  (void)dso;
  return 0;
}
