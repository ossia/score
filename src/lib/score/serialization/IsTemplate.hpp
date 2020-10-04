#pragma once
#include <cinttypes>
#include <type_traits>

// Check
// https://stackoverflow.com/questions/39812789/is-there-any-way-of-detecting-arbitrary-template-classes-that-mix-types-and-non
// again wiht c++20
template <class T>
struct is_template : std::false_type
{
};

template <template <class...> class T, class... Args>
struct is_template<T<Args...>> : std::true_type
{
};

template <template <auto...> class T, auto... Args>
struct is_template<T<Args...>> : std::true_type
{
};

template <template <class, auto, auto...> class T, class Arg1, auto Arg2, auto... Arg3>
struct is_template<T<Arg1, Arg2, Arg3...>> : std::true_type
{
};
template <template <auto, class, class...> class T, auto Arg1, class Arg2, class... Arg3>
struct is_template<T<Arg1, Arg2, Arg3...>> : std::true_type
{
};

template <template <class, auto, class> class T, class Arg1, auto Arg2, class Arg3>
struct is_template<T<Arg1, Arg2, Arg3>> : std::true_type
{
};

template <
    template <class, auto, class, class>
    class T,
    class Arg1,
    auto Arg2,
    class Arg3,
    class Arg4>
struct is_template<T<Arg1, Arg2, Arg3, Arg4>> : std::true_type
{
};

template <
    template <class, auto, class, class, class>
    class T,
    class Arg1,
    auto Arg2,
    class Arg3,
    class Arg4,
    class Arg5>
struct is_template<T<Arg1, Arg2, Arg3, Arg4, Arg5>> : std::true_type
{
};
template <template <auto, class, auto> class T, auto Arg1, class Arg2, auto Arg3>
struct is_template<T<Arg1, Arg2, Arg3>> : std::true_type
{
};

template <
    template <class, class, class, class, auto, auto, class>
    class T,
    class A1,
    class A2,
    class A3,
    class A4,
    auto A5,
    auto A6,
    class A7>
struct is_template<T<A1, A2, A3, A4, A5, A6, A7>> : std::true_type
{
};

template <
    template <class, class, class, class, class, auto, auto, class>
    class T,
    class A1,
    class A2,
    class A3,
    class A4,
    class A5,
    auto A6,
    auto A7,
    class A8>
struct is_template<T<A1, A2, A3, A4, A5, A6, A7, A8>> : std::true_type
{
};

#if defined(__clang__)
template <template<class, std::size_t> class T, class X, std::size_t u>
struct is_template<T<X,u>> : std::true_type
{
};
#endif

template <class>
struct is_custom_serialized : std::false_type
{
};
