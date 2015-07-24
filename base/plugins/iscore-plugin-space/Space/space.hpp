#pragma once
#include <ginac/ginac.h>

namespace spacelib
{
class space
{
    public:
        using variable_lst = std::vector<GiNaC::symbol>;

        space(const variable_lst& vars):
            m_variables(vars)
        {

        }

        const variable_lst& variables() const
        { return m_variables; }

    private:
        variable_lst m_variables;
};
}
