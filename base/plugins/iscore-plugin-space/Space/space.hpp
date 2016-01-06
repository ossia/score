#pragma once
#include <ginac/ginac.h>
#include <Space/bounded_symbol.hpp>
#include <QObject>
#include <array>
namespace spacelib
{
template<typename Symbol, int N>
class space_t
{
    public:
        static constexpr int dimension() { return N; }
        using variable_lst = std::array<Symbol, N>;

        space_t() = default;

        space_t(const variable_lst& vars):
            m_variables(vars)
        {

        }

        space_t(variable_lst&& vars):
            m_variables(std::move(vars))
        {

        }

        const variable_lst& variables() const
        { return m_variables; }

    private:
        variable_lst m_variables;
};

// TODO wrap GiNaC::symbol?
template<int N>
using space = space_t<GiNaC::symbol, N>;

template<int N, typename... Args>
using bounded_space = space_t<bounded_symbol<Args...>, N>;


template<typename Symbol>
class dynamic_space
{
    public:
        using symbol_type = Symbol;
        using variable_lst = std::vector<Symbol>;

        template<typename... T>
        dynamic_space(T&&... vars):
            m_variables{std::forward<T>(vars)...}
        {

        }

        int dimension() const
        { return m_variables.size(); }

        const variable_lst& variables() const
        { return m_variables; }

    private:
        variable_lst m_variables;
};

template<typename... Args>
using dynamic_bounded_space = dynamic_space<bounded_symbol<Args...>>;

using euclidean_space = dynamic_space<minmax_symbol>;

template<int N, typename DynamicSpace>
auto toStaticSpace(const DynamicSpace& s)
{
    ISCORE_ASSERT(s.variables().size() == N);

    std::array<typename DynamicSpace::symbol_type, N> arr;
    std::copy(begin(s.variables()), end(s.variables()), begin(arr));

    return space_t<typename DynamicSpace::symbol_type, N>(std::move(arr));
}
}


template<template<class...> class T, typename... Args>
static auto make(Args&&... args)
{
    return T<Args...>(std::forward<Args>(args)...);
}
