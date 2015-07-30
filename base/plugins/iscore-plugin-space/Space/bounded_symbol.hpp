#pragma once

namespace spacelib
{
template<
    typename Symbol,
    typename Domain>
class bounded_symbol
{
    public:
        bounded_symbol(const Symbol& sym,
                       const Domain& dom):
            m_sym{sym},
            m_dom{dom}
        {

        }

        bool validate() const
        {
            return m_dom.validate(m_sym);
        }

        auto& symbol() { return m_sym; }
        auto& domain() { return m_dom; }

    private:
        Symbol m_sym;
        Domain m_dom;
};
}
