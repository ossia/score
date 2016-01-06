#pragma once
#include <Space/area.hpp>
#include <Space/square_approximator.hpp>
#include <Space/dimension_iterator.hpp>
namespace spacelib
{
template<typename Size, typename Device>
class square_renderer
{
    public:
        Device render_device;
        const int side = 5.; // square size
        Size size; //complete size.

        /*
        // TODO make a render_dynamic which takes the sizes in argument, etc etc
        void render(const valued_area& a, const space<2>& s)
        {
            auto dim = make<dimension_apply>(s, square_approx_factory<800, 5>(),
            [&] (const GiNaC::exmap& map) {
                if(a.check(map))
                {
                    render_device.add(
                            GiNaC::ex_to<GiNaC::numeric>(map.at(s.variables()[0])).to_double() - side/2.,
                            GiNaC::ex_to<GiNaC::numeric>(map.at(s.variables()[1])).to_double() - side/2.,
                            double(side), double(side));
                }
            });

            dim.rec();
        }*/

        // Space should be a bounded one ?
        template<typename Space>
        void render(const valued_area& a, const Space& s)
        {
            auto dim = make<dimension_apply>(s, dynamic_square_approx_factory<Space>(s),
            [&] (const GiNaC::exmap& map) {
                if(a.check(map))
                {
                    render_device.add(
                            GiNaC::ex_to<GiNaC::numeric>(map.at(s.variables()[0])).to_double() - side/2.,
                            GiNaC::ex_to<GiNaC::numeric>(map.at(s.variables()[1])).to_double() - side/2.,
                            double(side), double(side));
                }
            });

            dim.rec();
        }
};

template<typename Device>
class dynamic_square_renderer
{
    public:
        Device render_device;
        const int side = 5.; // square size
};
}
