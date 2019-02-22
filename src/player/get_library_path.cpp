// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "get_library_path.hpp"

#if defined(__linux__)
#include <dlfcn.h>
#include <link.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_MSC_VER)
#include <psapi.h>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#endif
namespace score
{

#if defined(__linux__)
std::string get_library_path(ossia::string_view name_part)
{
  struct
  {
    std::string res;
    ossia::string_view name_part;
  } dat{{}, name_part};

  auto fun = [](dl_phdr_info* info, size_t s, void* d) {
    auto& ldat = *static_cast<decltype(dat)*>(d);
    auto path = ossia::string_view(info->dlpi_name);
    auto pos = path.find(ldat.name_part);
    if (pos != std::string::npos)
    {
      auto substr = path.substr(0, pos);
      ldat.res = std::string(substr.begin(), substr.end());
    }
    return 0;
  };

  dl_iterate_phdr(fun, &dat);

  return dat.res;
}

#elif defined(__APPLE__)

std::string get_library_path(ossia::string_view name_part)
{
  std::string s;
  auto num_libs = _dyld_image_count();
  for (unsigned int i = 0; i < num_libs; i++)
  {
    auto path = ossia::string_view(_dyld_get_image_name(i));
    auto pos = path.find(name_part);
    if (pos != std::string::npos)
    {
      auto substr = path.substr(0, pos);
      s = std::string(substr.begin(), substr.end());
      return s;
    }
  }
  return s;
}

#elif defined(_MSC_VER)

std::string get_library_path(ossia::string_view name_part)
{
  std::string s;
  // TODO :
  // https://msdn.microsoft.com/fr-fr/library/windows/desktop/ms682621(v=vs.85).aspx
  return s;
}
#endif
}
