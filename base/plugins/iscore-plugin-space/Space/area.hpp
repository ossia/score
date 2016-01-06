#pragma once
#include <QDebug>
#include <Space/space.hpp>

namespace spacelib
{
class area
{
        friend class projected_area;
    public:
        area(std::vector<GiNaC::relational>&& e,
             const std::vector<GiNaC::symbol>& vars):
            m_rels{std::move(e)},
            m_symbols(vars)
        {

        }

        std::vector<GiNaC::symbol> symbols() const
        {
            return m_symbols;
        }

        const auto& rels() const
        { return m_rels; }

    private:
        const std::vector<GiNaC::relational> m_rels;

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
        std::vector<GiNaC::relational> m_rels;

    public:
        projected_area(
                const area& a,
                const GiNaC::exmap& map)
        {
            // We substitute sybmols with other symbols corresponding to our space.
            m_rels.reserve(a.rels().size());
            for(auto& rel : a.rels())
            {
                m_rels.push_back(GiNaC::ex_to<GiNaC::relational>(rel.subs(map)));
            }
        }

        const auto& rels() const
        { return m_rels; }
};

// A projected area with all the parameters replaced by values
class valued_area
{
        std::vector<GiNaC::relational> m_rels;

    public:
        valued_area() = default;
        valued_area(const valued_area& other) = default;
        valued_area(valued_area&& other) = default;

        valued_area(
                const projected_area& a,
                const GiNaC::exmap& map)
        {
            m_rels.reserve(a.rels().size());
            for(auto& rel : a.rels())
            {
                m_rels.push_back(GiNaC::ex_to<GiNaC::relational>(rel.subs(map)));
            }

        }


        // a map of the space parameters (e.g. x, y) to values
        bool check(const GiNaC::exmap& map) const
        try
        {
            return std::accumulate(m_rels.begin(), m_rels.end(),
                                   true,
                                   [&] (bool cur, const GiNaC::relational& rel)
            {
                return cur && bool(GiNaC::ex_to<GiNaC::relational>(rel.subs(map)));
            });
        }
        catch(const std::exception& e)
        {
            qDebug() << e.what();
            return false;
        }
};

}
