#include "Space/area.hpp"
#include "Space/space.hpp"
#include "Space/computation.hpp"
#include "Space/square_renderer.hpp"
#include "Space/computation.hpp"
#include <iostream>
#include <memory>
#include <utility>

using namespace spacelib;
void t1()
{

    space<2> s({GiNaC::symbol("x")});

    for(auto elt : square_approx<100, 10>(s, 1))
    {
        std::cout << elt << " ";
    }
}

void t2()
{
    using namespace GiNaC;
    // Circle overlap
    space<2> s({GiNaC::symbol("x"), GiNaC::symbol("y")});

    symbol xv_a0("xv");
    symbol yv_a0("yv");
    symbol x0_a0("x0");
    symbol y0_a0("y0");
    symbol r_a0("r");
    area a0{pow((xv_a0 - x0_a0),2) + pow((yv_a0 - y0_a0),2) <= pow(r_a0,2),
            {xv_a0, yv_a0, x0_a0, y0_a0, r_a0}/*,
            {{x0_a0, numeric(100)}, {y0_a0, numeric(100)}, {r_a0, 10}}*/};


    symbol xv_a1("xv");
    symbol yv_a1("yv");
    symbol x0_a1("x0");
    symbol y0_a1("y0");
    symbol r_a1("r");
    area a1{pow((xv_a1 - x0_a1),2) + pow((yv_a1 - y0_a1),2) <= pow(r_a1,2),
            {xv_a1, yv_a1, x0_a1, y0_a1, r_a1}/*,
            {{x0_a1, numeric(100)}, {y0_a1, numeric(105)}, {r_a1, 10}}*/};

    projected_area pa0(a0, {{xv_a0, s.variables()[0]}, {yv_a0, s.variables()[1]}} );
    projected_area pa1(a1, {{xv_a1, s.variables()[0]}, {yv_a1, s.variables()[1]}} );

    valued_area va0(pa0,  {{x0_a0, numeric(100)}, {y0_a0, numeric(100)}, {r_a0, 10}});
    valued_area va1(pa1,  {{x0_a1, numeric(100)}, {y0_a1, numeric(105)}, {r_a1, 10}});
    qDebug() << overlap_computation::evaluate(va0, va1, s, square_approx<800, 5>(s, 0));

    // Note : if we have equalities no need to aprroximate, we just replace.
    // hence we should strongly-type relations?

    // Todo : restrict computations to an area so that we can do z = x^2 + y^2 ony in a circle for instance. -> it's a kind of mapping.
}

enum class RelType
{
    Different, Equal, Inferior, InferiorEqual, Superior, SuperiorEqual
};

class area_parser
{
        GiNaC::parser m_reader;
        GiNaC::ex m_res;
    public:
        area_parser(const std::string& str)
        { m_res = m_reader(str); }

        bool check() const
        { return GiNaC::is_a<GiNaC::relational>(m_res); }

        area result() const
        {
            auto rel = GiNaC::ex_to<GiNaC::relational>(m_res);

            area a(rel, {});

        }
};

spacelib::area make_area(const std::string& r1)
{
    using namespace GiNaC;
    parser reader;
    ex r = reader(r1);
    bool is_rel = GiNaC::is_a<GiNaC::relational>(r);

    /*

    ex e1 = reader(r1); // Error handling ?
    ex e2 = reader(r2); // Error handling ? What happens if the same symbol is on both sides ?
    relational r;

    switch(operatorp)
    {

    }

    symtab table = reader.get_syms();
    */
    // area a(e, {}, {});

}

void t3()
{
    // Parsing
    using namespace GiNaC;
    parser reader;
    ex e = reader("2*x+sin(y)");
    symtab table = reader.get_syms();
    symbol x = GiNaC::ex_to<GiNaC::symbol>(table["x"]);
    symbol y = GiNaC::ex_to<GiNaC::symbol>(table["y"]);

    // Have an abstract area that runs with a variable number of space variables, and a fixed one afterwards ?
}

int main()
{   t2();
    t3();

    return 0;
}
