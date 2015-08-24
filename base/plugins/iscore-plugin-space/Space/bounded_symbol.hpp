#pragma once
#include <ginac/ginac.h>
namespace spacelib
{
// Domain concept :
/*
concept Domain
{
    bool validate(Symbol s)
    {
        // True if evalf(s) is in the currently defined domain.
    }
};
*/

struct MinMaxDomain
{
        double min = 0;
        double max = 800;

        template<typename Symbol>
        bool validate(const Symbol& s)
        {
            double val = GiNaC::ex_to<GiNaC::numeric>(s.evalf()).to_double();
            return val >= min && val <= max;
        }
};

template<
    typename Symbol,
    typename Domain>
class bounded_symbol
{
    public:
        bounded_symbol() = default;
        bounded_symbol(const Symbol& sym,
                       const Domain& dom):
            m_sym{sym},
            m_dom(dom)
        {

        }

        bool validate() const
        {
            return m_dom.validate(m_sym);
        }

        // The value should only be set in a valid range ?

        Symbol& symbol() { return m_sym; }
        Domain& domain() { return m_dom; }
        const Symbol& symbol() const { return m_sym; }
        const Domain& domain() const { return m_dom; }

    private:
        Symbol m_sym;
        Domain m_dom;
};

using minmax_symbol = spacelib::bounded_symbol<GiNaC::symbol, spacelib::MinMaxDomain>;
}
