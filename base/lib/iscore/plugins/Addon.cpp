#include "Addon.hpp"

namespace iscore
{
ISCORE_LIB_BASE_EXPORT
QString addonArchitecture()
{
#if defined(_WIN32)
  return "windows-x86";
#elif defined(__linux__)
  return "linux-amd64";
#elif defined(__APPLE__) && defined(__MACH__)
  return "darwin-amd64";
#else
  return "undefined";
#endif
}
}
