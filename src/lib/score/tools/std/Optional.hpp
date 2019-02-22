#pragma once
#include <ossia/detail/optional.hpp>

template <typename... Args>
using optional = ossia::optional<Args...>;
using none_t = decltype(ossia::none);
