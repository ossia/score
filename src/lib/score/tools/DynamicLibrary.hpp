#pragma once
#include <ossia/detail/dylib_loader.hpp>

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace score
{

/**
 * @brief dlopen a library, reporting absence instead of throwing.
 *
 * ossia::dylib_loader only signals failure by throwing from its constructor,
 * which makes it unusable as a direct member of the optional-dependency
 * loaders: a function-try-block around a constructor rethrows when it falls
 * off the end of the handler, so the "unavailable" flag those loaders check is
 * never reached and the exception escapes their instance() accessor instead.
 */
[[nodiscard]] inline std::optional<ossia::dylib_loader>
try_load_library(const char* name) noexcept
{
  try
  {
    return ossia::dylib_loader{name};
  }
  catch(...)
  {
    return std::nullopt;
  }
}

//! Loads the first of @p names that can be found
[[nodiscard]] inline std::optional<ossia::dylib_loader>
try_load_library(std::vector<std::string_view> names) noexcept
{
  try
  {
    return ossia::dylib_loader{std::move(names)};
  }
  catch(...)
  {
    return std::nullopt;
  }
}

}
