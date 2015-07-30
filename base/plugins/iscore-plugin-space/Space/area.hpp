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

        equation map_to_space(const space<2>& s) const
        {
            // Space dim must be >= to variables dim
            Q_ASSERT(s.variables().size() >= m_variables.size());

            // We affect all the space dimensions we can to the
            // dimensions here.
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

        const variable_lst& variables() const
        { return m_variables; }

        const parameter_map& parameters() const
        { return m_parameters; }

    private:
        equation m_exp;

        // Variables that can map to a dimension of space
        variable_lst m_variables;

        // Map between symbol and numeric value.
        parameter_map m_parameters;
};
}
