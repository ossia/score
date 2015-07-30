#pragma once
#include <ginac/ginac.h>

namespace spacelib
{
template<typename Symbol>
class space_t
{
    public:
        using variable_lst = std::vector<Symbol>;

        space_t(const variable_lst& vars):
            m_variables(vars)
        {

        }

        const variable_lst& variables() const
        { return m_variables; }

    private:
        variable_lst m_variables;
};

using space = space_t<GiNaC::symbol>;

template<
    typename Symbol,
    typename Domain>
class bounded_symbol
{
    public:
        bounded_symbol(Symbol sym,
                       Domain dom):
            m_sym{sym},
            m_dom{dom}
        {

        }

        auto validate() const
        {
            return m_dom.validate(m_sym);
        }

        auto& symbol() { return m_sym; }
        auto& domain() { return m_dom; }

    private:
        Symbol m_sym;
        Domain m_dom;
};

template<typename... Args>
using bounded_space = space_t<bounded_symbol<Args...>>;
}


template<template<class...> typename T, typename... Args>
static auto make(Args&&... args)
{
    return T<Args...>(std::forward<Args>(args)...);
}
