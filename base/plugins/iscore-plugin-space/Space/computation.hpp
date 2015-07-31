#pragma once
#include <Space/area.hpp>
#include <Space/dimension_iterator.hpp>
#include <Space/square_approximator.hpp>
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
    public:
        virtual ~computation() = default;
};

class value_computation : public computation
{
    public:
        GiNaC::ex f;

        double evaluate(GiNaC::exmap values) const
        {
            return GiNaC::ex_to<GiNaC::numeric>(f.subs(values).evalf()).to_double();
        }
};


class overlap_computation : public computation
{
    public:
        template<typename Space, typename Approximator>
        static bool evaluate(
                const valued_area& m1,
                const valued_area& m2,
                const Space& s,
                const Approximator& approx)
        {
            // 1. Map space to a1, a2 free variables.
            // 2. For each value of each approximated dimension of the area
            //       Check if we have a1.has(val) && a2.has(val)

            // Note : all the other parameters have to have a value assigned.

            // TODO : first try to replace and simplify? (e.g. if it's a linear system)
            // And try to iterate only if it's not possible.

            auto dim = make<dimension_eval>(s, approx,
            [&] (const GiNaC::exmap& map) {
                return m1.check(map) && m2.check(map);
            });

            return dim.rec();
        }
};

}
