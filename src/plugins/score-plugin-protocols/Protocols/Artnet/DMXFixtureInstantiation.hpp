#pragma once
#include <ossia/detail/config.hpp>

#if defined(OSSIA_PROTOCOL_ARTNET)

namespace ossia::net
{
class generic_device;
struct dmx_buffer;
}
namespace Protocols
{
namespace Artnet
{
struct Fixture;
}
void addArtnetFixture(
    ossia::net::generic_device& dev, ossia::net::dmx_buffer& buffer,
    const Artnet::Fixture& fix, int channels_per_universe);
}
#endif
