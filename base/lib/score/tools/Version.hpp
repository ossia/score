#pragma once
#include <cstdint>

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
  explicit Version(int32_t v) : m_impl{v}
  {
  }
  Version(const Version&) = default;
  Version(Version&&) = default;
  Version& operator=(const Version&) = default;
  Version& operator=(Version&&) = default;

  bool operator==(Version other) const
  {
    return m_impl == other.m_impl;
  }
  bool operator!=(Version other) const
  {
    return m_impl != other.m_impl;
  }
  bool operator<(Version other) const
  {
    return m_impl < other.m_impl;
  }
  bool operator>(Version other) const
  {
    return m_impl > other.m_impl;
  }
  bool operator<=(Version other) const
  {
    return m_impl <= other.m_impl;
  }
  bool operator>=(Version other) const
  {
    return m_impl >= other.m_impl;
  }

  int32_t value() const
  {
    return m_impl;
  }

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
  auto operator()(const score::Version& s) const
  {
    return s.value();
  }
};
}
