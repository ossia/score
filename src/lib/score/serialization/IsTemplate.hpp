#pragma once
#include <score/config.hpp>

#include <cinttypes>
#include <type_traits>

// Check
// https://stackoverflow.com/questions/39812789/is-there-any-way-of-detecting-arbitrary-template-classes-that-mix-types-and-non
// again with c++20
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
  requires(sizeof...(Arg3) > 0)
struct is_template<T<Arg1, Arg2, Arg3...>> : std::true_type
{
};
template <template <auto, class, class...> class T, auto Arg1, class Arg2, class... Arg3>
  requires(sizeof...(Arg3) > 0)
struct is_template<T<Arg1, Arg2, Arg3...>> : std::true_type
{
};

template <template <class, auto, class> class T, class Arg1, auto Arg2, class Arg3>
struct is_template<T<Arg1, Arg2, Arg3>> : std::true_type
{
};

template <
    template <class, auto, class, class> class T, class Arg1, auto Arg2, class Arg3,
    class Arg4>
struct is_template<T<Arg1, Arg2, Arg3, Arg4>> : std::true_type
{
};

template <
    template <class, auto, class, class, class> class T, class Arg1, auto Arg2,
    class Arg3, class Arg4, class Arg5>
struct is_template<T<Arg1, Arg2, Arg3, Arg4, Arg5>> : std::true_type
{
};
template <template <auto, class, auto> class T, auto Arg1, class Arg2, auto Arg3>
struct is_template<T<Arg1, Arg2, Arg3>> : std::true_type
{
};

template <
    template <class, class, class, class, auto, auto, class> class T, class A1, class A2,
    class A3, class A4, auto A5, auto A6, class A7>
struct is_template<T<A1, A2, A3, A4, A5, A6, A7>> : std::true_type
{
};

template <
    template <class, class, class, class, class, auto, auto, class> class T, class A1,
    class A2, class A3, class A4, class A5, auto A6, auto A7, class A8>
struct is_template<T<A1, A2, A3, A4, A5, A6, A7, A8>> : std::true_type
{
};

template <template <class, auto> class T, class X, auto u>
struct is_template<T<X, u>> : std::true_type
{
};

template <template <class, class, bool> class T, class A, class B, bool C>
struct is_template<T<A, B, C>> : std::true_type
{
};

template <
    template <class, class, class, class, class, class, bool> class T, class A1,
    class A2, class A3, class A4, class A5, class A6, bool B>
struct is_template<T<A1, A2, A3, A4, A5, A6, B>> : std::true_type
{
};

template <
    template <class, class, class, class, class, class, class, bool> class T, class A1,
    class A2, class A3, class A4, class A5, class A6, class A7, bool B>
struct is_template<T<A1, A2, A3, A4, A5, A6, A7, B>> : std::true_type
{
};

template <class>
struct is_custom_serialized : std::false_type
{
};
