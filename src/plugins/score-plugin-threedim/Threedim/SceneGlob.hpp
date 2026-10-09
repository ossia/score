#pragma once
#include <cstddef>
#include <string_view>

namespace Threedim
{

// Path glob of the scene nodes (Scene Graph Filter, Scene Selector,
// Configure Primitive): `*` matches anything except `/`, `**` matches across
// slashes, `?` matches a single non-slash character, everything else is
// literal.
inline bool scene_glob_match(std::string_view pattern, std::string_view text) noexcept
{
  constexpr auto npos = std::string_view::npos;
  std::size_t pi = 0, ti = 0;
  // Where to resume after a mismatch: the last `*`, which cannot eat a slash,
  // and the last `**`, which a `*` stopped by a slash falls back to.
  std::size_t star_pi = npos, star_ti = 0;
  std::size_t dstar_pi = npos, dstar_ti = 0;

  while(ti < text.size())
  {
    if(pi < pattern.size())
    {
      const char pc = pattern[pi];
      if(pc == '*')
      {
        if(pi + 1 < pattern.size() && pattern[pi + 1] == '*')
        {
          pi += 2;
          dstar_pi = pi;
          dstar_ti = ti;
          star_pi = npos;
        }
        else
        {
          pi += 1;
          star_pi = pi;
          star_ti = ti;
        }
        continue;
      }
      if(pc == '?' ? text[ti] != '/' : pc == text[ti])
      {
        ++pi;
        ++ti;
        continue;
      }
    }
    if(star_pi != npos && text[star_ti] != '/')
    {
      pi = star_pi;
      ti = ++star_ti;
      continue;
    }
    if(dstar_pi != npos)
    {
      star_pi = npos;
      pi = dstar_pi;
      ti = ++dstar_ti;
      continue;
    }
    return false;
  }
  while(pi < pattern.size() && pattern[pi] == '*')
    ++pi;
  return pi == pattern.size();
}

}
