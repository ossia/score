#pragma once
#ifdef __has_include
#  if __has_include(<optional>)
#    include <optional>
namespace iscore {
template<typename... Args> using optional = std::optional<Args...>;
const constexpr auto none = std::nullopt;
}
#  elif __has_include(<experimental/optional>)
#    include <experimental/optional>
namespace iscore {
template<typename... Args> using optional = std::experimental::optional<Args...>;
const constexpr auto none = std::experimental::nullopt;
}
#  else
#    include <boost/optional.hpp>
namespace iscore {
template<typename... Args> using optional = boost::optional<Args...>;
const constexpr auto none = boost::none;
}
#  endif
#else
#    include <boost/optional.hpp>
namespace iscore {
template<typename... Args> using optional = boost::optional<Args...>;
const constexpr auto none = boost::none;
}
#endif

//TODO
template<typename... Args>
using optional = iscore::optional<Args...>;
