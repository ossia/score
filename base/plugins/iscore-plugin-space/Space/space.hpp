#pragma once
#include <ginac/ginac.h>
#include <array>
#include <Space/bounded_symbol.hpp>

namespace spacelib
{
template<typename Symbol, int N>
class space_t
{
    public:
        static const constexpr int dimension{N};
        using variable_lst = std::array<Symbol, N>;

        space_t(const variable_lst& vars):
            m_variables(vars)
        {

        }

        const variable_lst& variables() const
        { return m_variables; }

    private:
        variable_lst m_variables;
};

template<int N>
using space = space_t<GiNaC::symbol, N>;

template<int N, typename... Args>
using bounded_space = space_t<bounded_symbol<Args...>, N>;
}


template<template<class...> class T, typename... Args>
static auto make(Args&&... args)
{
    return T<Args...>(std::forward<Args>(args)...);
}
