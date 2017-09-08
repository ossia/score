#pragma once
#include <type_traits>

template <class>
struct is_template : std::false_type
{
};

template <template <class...> class T, typename... Args>
struct is_template<T<Args...>> : std::true_type
{
};

template <class>
struct is_custom_serialized : std::false_type
{
};
