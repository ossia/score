#pragma once
#include <ossia/detail/logger.hpp>

namespace oscr
{
struct Logger
{
  using logger_type = Logger;
  template <typename... T>
  static void log(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    ossia::logger().info(fmt, std::forward<T>(args)...);
  }
  template <typename... T>
  static void trace(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    ossia::logger().trace(fmt, std::forward<T>(args)...);
  }
  template <typename... T>
  static void debug(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    ossia::logger().debug(fmt, std::forward<T>(args)...);
  }
  template <typename... T>
  static void info(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    ossia::logger().info(fmt, std::forward<T>(args)...);
  }
  template <typename... T>
  static void warn(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    ossia::logger().warn(fmt, std::forward<T>(args)...);
  }
  template <typename... T>
  static void error(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    ossia::logger().error(fmt, std::forward<T>(args)...);
  }
  template <typename... T>
  static void critical(fmt::format_string<T...> fmt, T&&... args) noexcept
  {
    ossia::logger().critical(fmt, std::forward<T>(args)...);
  }
};
}
