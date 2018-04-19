// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Addon.hpp"

namespace score
{
SCORE_LIB_BASE_EXPORT
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
