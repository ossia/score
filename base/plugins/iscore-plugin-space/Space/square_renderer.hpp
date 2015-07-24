#pragma once
#include <Space/area.hpp>

namespace spacelib
{
template<typename Size, typename Device>
class square_renderer
{
    public:
        Device render_device;
        double side = 5.; // square size
        Size size; //complete size.

        // How to make this dimension-agnostic ?
        void render(const area& a, const space& s)
        {
            GiNaC::exmap var_map;
            auto eqn = a.map_to_space(s);

            for(int x = 0; x < size.x(); x += side)
            {
                var_map[s.variables()[0]] = x;
                for(int y = 0; y < size.y(); y += side)
                {
                    var_map[s.variables()[1]] = y;

                    if(area::check(eqn, a.parameters(), var_map))
                    {
                        render_device.add(x-side/2., y-side/2., side, side);
                    }
                }
            }
        }
};
}
