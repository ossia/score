#pragma once
#include <ossia/detail/config.hpp>

#if defined(OSSIA_PROTOCOL_ARTNET)
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

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
    const Artnet::Fixture& fix, int start_universe, int channels_per_universe);

int requiredUniverses(
    std::span<const Artnet::Fixture> fixtures, int start_universe,
    int channels_per_universe) noexcept;

namespace Artnet
{
int usedChannels(const Fixture& fixture) noexcept;
void normalizeAddress(Fixture& fixture, int channels_per_universe) noexcept;
std::pair<int, int>
nextAvailableAddress(std::span<const Fixture> fixtures, int channels_per_universe) noexcept;
std::pair<int, int>
fitInUniverse(int universe, int64_t address, int channels, int channels_per_universe) noexcept;
std::vector<Fixture> repeatFixture(
    Fixture fixture, int address, int count, int spacing, int channels_per_universe);
}
}
#endif
