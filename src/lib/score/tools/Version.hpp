#pragma once
#include <cstdint>
#include <functional>

namespace score
{
/**
 * @brief Represents the version of a plug-in.
 *
 * Used for save file updating.
 */
class Version
{
public:
  explicit constexpr Version(int32_t v) noexcept
      : m_impl{v}
  {
  }
  constexpr Version(const Version&) = default;
  constexpr Version(Version&&) = default;
  constexpr Version& operator=(const Version&) = default;
  constexpr Version& operator=(Version&&) = default;

  constexpr bool operator==(Version other) const noexcept
  {
    return m_impl == other.m_impl;
  }
  constexpr bool operator!=(Version other) const noexcept
  {
    return m_impl != other.m_impl;
  }
  constexpr bool operator<(Version other) const noexcept
  {
    return m_impl < other.m_impl;
  }
  constexpr bool operator>(Version other) const noexcept
  {
    return m_impl > other.m_impl;
  }
  constexpr bool operator<=(Version other) const noexcept
  {
    return m_impl <= other.m_impl;
  }
  constexpr bool operator>=(Version other) const noexcept
  {
    return m_impl >= other.m_impl;
  }

  constexpr int32_t value() const noexcept { return m_impl; }

private:
  int32_t m_impl = 0;
};
}

namespace std
{
template <>
struct hash<score::Version>
{
public:
  std::size_t operator()(const score::Version& s) const noexcept { return s.value(); }
};
}
