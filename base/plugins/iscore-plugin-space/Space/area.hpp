#pragma once
#include <QDebug>
#include <Space/space.hpp>

namespace spacelib
{
class area
{
        friend class projected_area;
    public:
        area(const GiNaC::relational& e,
             const std::vector<GiNaC::symbol>& vars):
            m_rel{e},
            m_symbols(vars)
        {

        }

        std::vector<GiNaC::symbol> symbols() const
        {
            return m_symbols;
        }

        const GiNaC::relational& rel() const
        { return m_rel; }

    private:
        GiNaC::relational m_rel;

        // Variables that can map to a dimension of space
        std::vector<GiNaC::symbol> m_symbols;

        // Map between symbol and numeric value.
        //GiNaC::exmap m_parameters;
};



// An area projected on a space given a mapping.
// e.g. given
//  - the space [x, y]
//  - the area (a - b)² + (c - d)² = r²,
//  - the map {{a, 1}, {c, 2}}
// get (x - a)² + (y - d)² = r².
class projected_area
{
        GiNaC::relational m_rel;

        // Map between symbol and numeric value.
        GiNaC::exmap m_parameters;

    public:
        projected_area(
                const area& a,
                const GiNaC::exmap& map)
        {
            // We substitute sybmols with other symbols corresponding to our space.
            m_rel = GiNaC::ex_to<GiNaC::relational>(a.rel().subs(map));

            // All the symbols that weren't mapped are parameters.
            for(auto& sym : a.symbols())
            {
                if(map.find(sym) != map.end())
                    m_parameters.insert({sym, 0});
            }
        }

        const GiNaC::relational& rel() const
        { return m_rel; }
};

// A projected area with all the parameters replaced by values
class valued_area
{
        GiNaC::relational m_rel;

    public:
        valued_area(
                const projected_area& a,
                const GiNaC::exmap& map)
        {
            m_rel = GiNaC::ex_to<GiNaC::relational>(a.rel().subs(map));
        }


        // a map of the space parameters (e.g. x, y) to values
        bool check(const GiNaC::exmap& map) const
        {
            return bool(GiNaC::ex_to<GiNaC::relational>(m_rel.subs(map)));
        }
};

}
