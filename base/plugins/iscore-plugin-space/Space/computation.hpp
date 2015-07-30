#pragma once
#include <Space/area.hpp>

namespace spacelib
{
namespace rcc {
    struct disjoint;
    struct equal;
    struct overlap;
    struct proper_part;
}

// Takes areas, an equation, and a symbol in which we put the result.
// 2 cases :
//  1. result = x_0 + 1
//  2. result(x,y) = milieu de deux points ? Faut-il raisonner avec des addresses, ou avec des valeurs issues d'autres zones ?
// Plutôt des addresses : pas besoin de garder les mêmes variables ...
//  2. result = overlap(A_1, A_2)
//            = exists(X in A_1) such as X in A_2
//            = exists(X) | A_1(X) == A_2(X) == 1 (with X in space)

// Have a notion of point (x, y) of symbols that can be replaced?
class computation
{
        using formula = GiNaC::ex;
    public:
        formula e;

        template<typename Space>
        bool overlap(area a1, area a2, Space s)
        {
            return true;
        }
};


class overlap_computation
{
    public:
        template<typename Space, typename Approximator>
        bool evaluate(area a1, area a2, Space s, Approximator approx)
        {
            // 1. Map space to a1, a2 free variables.
            // 2. For each value of each approximated dimension of the area
            //       Check if we have a1.has(val) && a2.has(val)

            auto m1 = a1.map_to_space(s);
            auto m2 = a2.map_to_space(s);


        }
};

inline auto evaluate(computation c, GiNaC::exmap values)
{
    //area e1, e2;
    // Replace
    return GiNaC::ex_to<GiNaC::numeric>(c.e.subs(values).evalf()).to_double();
}

}
