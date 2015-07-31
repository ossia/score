#pragma once
#include <QDebug>
#include <Space/space.hpp>

namespace spacelib
{

class area
{
    public:
        using variable_lst = std::vector<GiNaC::symbol>;
        using parameter = GiNaC::symbol;
        using number = GiNaC::numeric;
        using parameter_map = GiNaC::exmap;
        using variable_map = GiNaC::exmap;
        using numlst = GiNaC::lst;
        using equation = GiNaC::relational;

        // Symbol1 : x, y
        // Symbol2 : p_0, p_1 : the parameters

        // ex: (x - p_0)² + (y - p_1)² <= p_2² for a disc
        // Should we have all variables readily accessible instead?
        area(const equation& e,
             const variable_lst& vars,
             const parameter_map& params):
            m_exp{e},
            m_variables(vars),
            m_parameters(params)
        {

        }

        // Set a parameter
        void set(const parameter& sym, const number& val)
        {
            m_parameters.at(sym) = val;
        }

        // Should take a mapping in argument, too. Maybge area should be mapped_area ?
        template<typename Space>
        equation map_to_space(const Space& s) const
        {
            // Space dim must be >= to variables dim
            Q_ASSERT(Space::dimension >= m_variables.size());

            // We affect all the space dimensions we can to the
            // dimensions here. This should be done by the user...
            GiNaC::exmap m;
            for(std::size_t i = 0; i < m_variables.size(); i++)
            {
                m.insert({m_variables[i], s.variables()[i]});
            }

            return GiNaC::ex_to<equation>(m_exp.subs(m));
        }

        static bool check(
                const equation& e,
                const variable_map& vars,
                const parameter_map& params)
        {
            auto m = params;
            m.insert(vars.begin(), vars.end());

            return bool(GiNaC::ex_to<equation>(e.subs(m)));
        }

        const parameter_map& parameters() const
        { return m_parameters; }

    private:
        equation m_exp;

        // Variables that can map to a dimension of space
        variable_lst m_variables;

        // Map between symbol and numeric value.
        parameter_map m_parameters;
};



// An area projected on a space given a mapping.
class projected_area
{
    public:
        template<typename Space>
        projected_area(const area& a, const Space& space, std::array<std::pair<GiNaC::symbol, int>, Space::dimension> map)
        {

        }

};

}
