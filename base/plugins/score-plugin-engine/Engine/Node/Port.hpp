#pragma once
#include <QString>
#include <cstdint>
#include <array>

namespace Process
{
struct AudioInInfo {
  const QLatin1String name;

  template<std::size_t N>
  constexpr AudioInInfo(const char (&name)[N]): name{name, N} { }
};
struct AudioOutInfo {
  const QLatin1String name;

  template<std::size_t N>
  constexpr AudioOutInfo(const char (&name)[N]): name{name, N} { }
};
struct ValueInInfo {
  const QLatin1String name;
  const bool is_event{};

  template<std::size_t N>
  constexpr ValueInInfo(const char (&name)[N]): name{name, N} { }

  template<std::size_t N>
  constexpr ValueInInfo(const char (&name)[N], bool b): name{name, N}, is_event{b} { }
};
struct ValueOutInfo {
  const QLatin1String name;

  template<std::size_t N>
  constexpr ValueOutInfo(const char (&name)[N]): name{name, N} { }
};
struct MidiInInfo {
  const QLatin1String name;

  template<std::size_t N>
  constexpr MidiInInfo(const char (&name)[N]): name{name, N} { }
};
struct MidiOutInfo {
  const QLatin1String name;

  template<std::size_t N>
  constexpr MidiOutInfo(const char (&name)[N]): name{name, N} { }
};
struct ControlInfo {
  const QLatin1String name;

  template<std::size_t N, typename... Args>
  constexpr ControlInfo(const char (&name)[N]):
    name{name, N}
  {
  }
};
template<typename... Args>
constexpr std::array<const char*, sizeof...(Args)> array(Args&&... args)
{
  return {args...};
}
}
