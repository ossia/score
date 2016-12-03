#pragma once
#if defined(__native_client__)
#define ISCORE_USE_BOOST_OPTIONAL 1
#elif defined(_MSC_VER)
#define ISCORE_USE_BOOST_OPTIONAL 1

#else

#ifdef __has_include
#if __has_include(<optional>)
#define ISCORE_USE_STD_OPTIONAL 1
#elif __has_include(<experimental/optional>)
#define ISCORE_USE_STD_EXPERIMENTAL_OPTIONAL 1
#else
#define ISCORE_USE_BOOST_OPTIONAL 1
#endif
#else
#define ISCORE_USE_BOOST_OPTIONAL 1
#endif
#endif

#if defined(ISCORE_USE_BOOST_OPTIONAL)
#include <boost/optional.hpp>
namespace iscore
{
template <typename... Args>
using optional = boost::optional<Args...>;
const auto none = boost::none;
}
#elif defined(ISCORE_USE_STD_OPTIONAL)
#include <optional>
namespace iscore
{
template <typename... Args>
using optional = std::optional<Args...>;
const constexpr auto none = std::nullopt;
}
#elif defined(ISCORE_USE_STD_EXPERIMENTAL_OPTIONAL)
#include <experimental/optional>
namespace iscore
{
template <typename... Args>
using optional = std::experimental::optional<Args...>;
const constexpr auto none = std::experimental::nullopt;
}
#else
#endif
// TODO
template <typename... Args>
using optional = iscore::optional<Args...>;
using none_t = decltype(iscore::none);
