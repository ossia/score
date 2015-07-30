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

        void render(const area& a, const space<2>& s)
        {
            auto eqn = a.map_to_space(s);

            auto dim = make<dimension_apply>(s, square_approx<800, 5>(s, 0),
            [&] (const GiNaC::exmap& map) {
                if(area::check(eqn, a.parameters(), map))
                {
                    render_device.add(
                            GiNaC::ex_to<GiNaC::numeric>(map.at(s.variables()[0])).to_double() - side/2.,
                            GiNaC::ex_to<GiNaC::numeric>(map.at(s.variables()[1])).to_double() - side/2.,
                            side, side);
                }
            });

            dim.rec();
        }
};
}
