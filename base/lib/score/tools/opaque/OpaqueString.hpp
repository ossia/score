#pragma once
#include <QString>
#include <functional>
#include <string>

class OpaqueString
{
  friend struct std::hash<OpaqueString>;
  friend bool operator==(const OpaqueString& lhs, const OpaqueString& rhs)
  {
    return lhs.impl == rhs.impl;
  }

  friend bool operator<(const OpaqueString& lhs, const OpaqueString& rhs)
  {
    return lhs.impl < rhs.impl;
  }

public:
  OpaqueString() = default;

  explicit OpaqueString(const char* str) noexcept : impl{str}
  {
  }
  explicit OpaqueString(std::string str) noexcept : impl{std::move(str)}
  {
  }
  explicit OpaqueString(const QString& str) noexcept : impl{str.toStdString()}
  {
  }

  explicit OpaqueString(const OpaqueString& str) noexcept : impl{str.impl}
  {
  }
  explicit OpaqueString(OpaqueString&& str) noexcept
      : impl{std::move(str.impl)}
  {
  }

  OpaqueString& operator=(const OpaqueString& str) noexcept
  {
    impl = str.impl;
    return *this;
  }
  OpaqueString& operator=(OpaqueString&& str) noexcept
  {
    impl = std::move(str.impl);
    return *this;
  }

protected:
  std::string impl;
};

namespace std
{
template <>
struct hash<OpaqueString>
{
  std::size_t operator()(const OpaqueString& kagi) const noexcept
  {
    return std::hash<std::string>()(kagi.impl);
  }
};
}
