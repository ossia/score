#pragma once
#include <ossia/detail/config.hpp>

#if defined(OSSIA_PROTOCOL_ARTNET)
#include <Protocols/Artnet/ArtnetSpecificSettings.hpp>

#include <ossia/protocols/artnet/dmx_protocol_base.hpp>

namespace Protocols
{
std::unique_ptr<ossia::net::dmx_protocol_base> instantiateDMXProtocol(
    const ossia::net::network_context_ptr& ctx, const ArtnetSpecificSettings& set);
}
#endif
