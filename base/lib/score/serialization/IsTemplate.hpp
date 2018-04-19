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

template <template <typename, std::size_t> class T, typename U, std::size_t M>
struct is_template<T<U, M>> : std::true_type
{
};

template <
    template <typename, std::size_t, typename> class T,
    typename U,
    std::size_t M,
    typename V>
struct is_template<T<U, M, V>> : std::true_type
{
};

template <class>
struct is_custom_serialized : std::false_type
{
};
