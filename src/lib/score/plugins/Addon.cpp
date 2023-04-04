// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Addon.hpp"

#include <boost/predef.h>

namespace score
{
SCORE_LIB_BASE_EXPORT
QString addonArchitecture()
{
  QString arch;
#if BOOST_OS_WINDOWS
  arch += "windows-";
#elif BOOST_OS_LINUX
  arch += "linux-";
#elif BOOST_OS_MACOS
  arch += "darwin-";
#else
  arch += "unknown-";
#endif

#if BOOST_ARCH_X86_64
  arch += "amd64";
#elif BOOST_ARCH_X86
  arch += "x86";
#elif BOOST_ARCH_ARM
#if BOOST_ARCH_WORD_BITS_64
  arch += "arm64";
#else
  arch += "arm";
#endif
#elif BOOST_ARCH_PPC64
  arch += "ppc64";
#elif BOOST_ARCH_PPC
  arch += "ppc";
#elif BOOST_ARCH_RISCV
  arch += "riscv";
#endif
  return arch;
}
}
