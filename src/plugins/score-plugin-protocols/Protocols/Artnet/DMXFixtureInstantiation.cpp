#include <ossia/detail/config.hpp>

#if defined(OSSIA_PROTOCOL_ARTNET)
#include "DMXFixtureInstantiation.hpp"

#include <Protocols/Artnet/ArtnetSpecificSettings.hpp>
#include <Protocols/Artnet/OpenFixtureLibrary.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/fmt.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/protocols/artnet/dmx_buffer.hpp>
#include <ossia/protocols/artnet/dmx_led_parameter.hpp>
#include <ossia/protocols/artnet/dmx_parameter.hpp>

#include <array>
#include <limits>

namespace Protocols
{

int Artnet::Channel::valueResolution() const noexcept
{
  if(resolution > 0)
    return std::min<int>(resolution, ossia::net::dmx_parameter::max_bytes);

  int64_t highest = defaultValue;
  if(auto* caps = ossia::get_if<std::vector<RangeCapability>>(&capabilities))
    for(const auto& c : *caps)
      highest = std::max<int64_t>(highest, c.range.second);
  return highest <= 0xFF ? 1 : highest <= 0xFFFF ? 2 : highest <= 0xFFFFFF ? 3 : 4;
}

namespace
{
struct ChannelLayout
{
  std::array<uint32_t, ossia::net::dmx_parameter::max_bytes> channels{};
  int bytes{};
};

int64_t maxValue(int bytes) noexcept
{
  return (int64_t{1} << (8 * bytes)) - 1;
}

constexpr int64_t max_led_channels = int64_t{1} << 25;

struct fixture_setup_visitor
{
  const Artnet::Channel& chan;
  ossia::net::node_base& fixt_node;
  ossia::net::dmx_buffer& buffer;
  const ChannelLayout& layout;

  int64_t toLayout(int64_t v) const noexcept
  {
    return OFL::scaleValue(v, chan.valueResolution(), layout.bytes);
  }

  int parameterMax() const noexcept
  {
    return int(
        std::min<int64_t>(maxValue(layout.bytes), std::numeric_limits<int>::max()));
  }

  int defaultValue() const noexcept
  {
    return int(std::min<int64_t>(toLayout(chan.defaultValue), parameterMax()));
  }

  ossia::net::dmx_parameter& createChannel() const
  {
    auto node = fixt_node.create_child(chan.name.toStdString());
    auto param = std::make_unique<ossia::net::dmx_parameter>(
        *node, buffer, std::span{layout.channels.data(), std::size_t(layout.bytes)}, 0,
        parameterMax());

    auto& p = *param;
    node->set_parameter(std::move(param));
    p.set_default_value(defaultValue());
    return p;
  }

  void operator()(const Artnet::SingleCapability& v) const
  {
    auto& p = createChannel();
    p.set_value(defaultValue());
    if(!v.comment.isEmpty())
      ossia::net::set_description(p.get_node(), v.comment.toStdString());
  }

  void operator()(const std::vector<Artnet::RangeCapability>& v) const
  {
    if(ossia::all_of(
           v, [](const auto& c) { return c.type == QLatin1String("NoFunction"); }))
      return;

    std::vector<std::pair<std::string, uint32_t>> values;
    values.reserve(v.size());
    ossia::hash_map<std::string, int> occurrences;
    occurrences.reserve(v.size());

    const int dv = chan.defaultValue;
    const int resolution = chan.valueResolution();
    int default_index = -1;
    std::string comment;
    for(const auto& capa : v)
    {
      const auto value = OFL::menuClickValue(capa, resolution, layout.bytes);
      if(!value)
        continue;

      std::string name
          = (capa.effectName.isEmpty() ? capa.type : capa.effectName).toStdString();
      if(int n = ++occurrences[name]; n > 1)
        name += fmt::format(" {}", n);

      if(default_index < 0 && capa.range.first <= dv && dv <= capa.range.second)
        default_index = values.size();

      if(!capa.comment.isEmpty())
        comment += fmt::format("{}: {}\n", name, capa.comment.toStdString());

      values.emplace_back(std::move(name), uint32_t(*value));
    }

    auto& chan_param = createChannel();
    auto& chan_node = chan_param.get_node();
    if(!comment.empty())
      ossia::net::set_description(chan_node, comment);

    if(values.empty())
    {
      chan_param.set_value(defaultValue());
      return;
    }

    auto preset_node = chan_node.create_child("preset");
    const std::string default_preset = values[std::max(default_index, 0)].first;
    auto preset = std::make_unique<ossia::net::dmx_enum_parameter>(
        *preset_node, chan_param, std::move(values));
    auto& p = *preset;
    preset_node->set_parameter(std::move(preset));
    p.set_default_value(default_preset);
    p.set_value(default_preset);

    if(!comment.empty())
      ossia::net::set_description(*preset_node, std::move(comment));

    chan_param.set_value(defaultValue());
  }
};

void writeConstant(
    ossia::net::dmx_buffer& buffer, const Artnet::Channel& chan,
    const ChannelLayout& layout)
{
  const auto v = OFL::scaleValue(chan.defaultValue, chan.valueResolution(), layout.bytes);
  for(int k = 0; k < layout.bytes; k++)
    buffer.write_channel(layout.channels[k], (v >> (8 * (layout.bytes - 1 - k))) & 0xFF);
}

struct led_visitor
{
  ossia::net::node_base& fixt_node;
  ossia::net::dmx_buffer& buffer;
  int dmx_channel;

  void create(int8_t bytes_per_pixel, int pixels, bool reverse = false) const
  {
    fixt_node.set_parameter(std::make_unique<ossia::net::dmx_led_parameter>(
        fixt_node, buffer, dmx_channel, bytes_per_pixel, pixels, reverse));
  }

  void operator()(const Artnet::LEDStripLayout& v) const
  {
    create(v.diodes.size(), v.length, v.reverse);
  }
  void operator()(const Artnet::LEDPaneLayout& v) const
  {
    create(v.diodes.size(), v.width * v.height);
  }
  void operator()(const Artnet::LEDVolumeLayout& v) const
  {
    create(v.diodes.size(), v.width * v.height * v.depth);
  }
  void operator()(ossia::monostate) const { }
};

int64_t fixtureBase(
    const Artnet::Fixture& fix, int start_universe, int channels_per_universe) noexcept
{
  const int universe = fix.universe < 0 ? 0 : std::max(0, fix.universe - start_universe);
  return int64_t(universe) * channels_per_universe + std::max(fix.address, 0);
}

void ensureUniverses(
    ossia::net::dmx_buffer& buffer, int64_t last_channel, int channels_per_universe)
{
  const int needed = 1 + last_channel / channels_per_universe;
  if(buffer.universes() < needed)
    buffer.set_universe_count(needed);
}
}

bool addArtnetFixture(
    ossia::net::generic_device& dev, ossia::net::dmx_buffer& buffer,
    const Artnet::Fixture& fix, int start_universe, int channels_per_universe,
    int max_universes)
{
  if(channels_per_universe <= 0)
    return false;
  if(fix.led && Artnet::usedChannels(fix) > max_led_channels)
    return false;

  const int64_t base = fixtureBase(fix, start_universe, channels_per_universe);
  auto fits = [&](int64_t last_channel) {
    return last_channel / channels_per_universe < max_universes;
  };

  if(fix.led)
  {
    const int64_t last = base + std::max(Artnet::usedChannels(fix), 1) - 1;
    if(!fits(last))
      return false;
    auto fixt_node = dev.create_child(fix.fixtureName.toStdString());
    if(!fixt_node)
      return false;
    ensureUniverses(buffer, last, channels_per_universe);
    ossia::visit(led_visitor{*fixt_node, buffer, int(base)}, fix.led);
    return true;
  }

  ossia::hash_map<QString, int> slots;
  slots.reserve(fix.mode.channelNames.size());
  for(int i = 0; i < std::ssize(fix.mode.channelNames); i++)
    if(const auto& name = fix.mode.channelNames[i]; !name.isEmpty())
      slots.try_emplace(name, i);

  auto slot = [&](const QString& name) {
    auto it = slots.find(name);
    return it != slots.end() ? it->second : -1;
  };

  std::vector<std::pair<const Artnet::Channel*, ChannelLayout>> layouts;
  layouts.reserve(fix.controls.size());
  int64_t last_channel = base;
  for(const auto& chan : fix.controls)
  {
    const int coarse = slot(chan.name);
    if(coarse < 0)
      continue;

    ChannelLayout layout;
    layout.channels[0] = base + coarse;
    layout.bytes = 1;
    const bool legacyRanges
        = chan.resolution == 0
          && ossia::get_if<std::vector<Artnet::RangeCapability>>(&chan.capabilities);
    for(const auto& fine : chan.fineChannels)
    {
      if(legacyRanges)
        break;
      const int s = slot(fine);
      if(s < 0 || layout.bytes == ossia::net::dmx_parameter::max_bytes)
        break;
      layout.channels[layout.bytes++] = base + s;
    }

    for(int k = 0; k < layout.bytes; k++)
      last_channel = std::max<int64_t>(last_channel, layout.channels[k]);
    layouts.emplace_back(&chan, layout);
  }

  if(!fits(last_channel))
    return false;
  auto fixt_node = dev.create_child(fix.fixtureName.toStdString());
  if(!fixt_node)
    return false;
  ensureUniverses(buffer, last_channel, channels_per_universe);

  for(const auto& [chan, layout] : layouts)
  {
    if(chan->constant)
      writeConstant(buffer, *chan, layout);
    else
      ossia::visit(
          fixture_setup_visitor{*chan, *fixt_node, buffer, layout}, chan->capabilities);
  }
  return true;
}

int requiredUniverses(
    std::span<const Artnet::Fixture> fixtures, int start_universe,
    int channels_per_universe, int max_universes) noexcept
{
  if(channels_per_universe <= 0)
    return 1;
  int64_t last = 0;
  for(const auto& fix : fixtures)
  {
    const int64_t size = Artnet::usedChannels(fix);
    if(fix.led && size > max_led_channels)
      continue;
    const int64_t end = fixtureBase(fix, start_universe, channels_per_universe)
                        + std::max<int64_t>(size, 1) - 1;
    if(end / channels_per_universe < max_universes)
      last = std::max(last, end);
  }
  return int(1 + last / channels_per_universe);
}

namespace Artnet
{
int usedChannels(const Fixture& fixture) noexcept
{
  if(!fixture.mode.channelNames.empty())
    return fixture.mode.channelNames.size();
  if(!fixture.controls.empty())
    return fixture.controls.size();
  return ossia::visit([](const auto& led) -> int {
    if constexpr(requires { led.channels(); })
      return led.channels();
    else
      return 0;
  }, fixture.led);
}

void normalizeAddress(Fixture& fixture, int channels_per_universe) noexcept
{
  if(channels_per_universe <= 0 || fixture.address < channels_per_universe)
    return;
  fixture.universe += fixture.address / channels_per_universe;
  fixture.address %= channels_per_universe;
}

std::pair<int, int>
fitInUniverse(int universe, int64_t address, int channels, int channels_per_universe) noexcept
{
  if(channels_per_universe <= 0)
    return {universe, int(std::clamp<int64_t>(address, 0, std::numeric_limits<int>::max()))};

  address = std::max<int64_t>(address, 0);
  int64_t u = universe + address / channels_per_universe;
  int64_t a = address % channels_per_universe;
  if(channels <= channels_per_universe && a + channels > channels_per_universe)
  {
    u++;
    a = 0;
  }
  return {int(std::min<int64_t>(u, std::numeric_limits<int>::max())), int(a)};
}

std::pair<int, int>
nextAvailableAddress(std::span<const Fixture> fixtures, int channels_per_universe) noexcept
{
  if(channels_per_universe <= 0)
    return {};

  int64_t end = 0;
  for(const auto& fix : fixtures)
    end = std::max(
        end, int64_t(fix.universe) * channels_per_universe + fix.address
                 + usedChannels(fix));

  return {int(end / channels_per_universe), int(end % channels_per_universe)};
}

std::vector<Fixture> repeatFixture(
    Fixture fixture, int address, int count, int spacing, int channels_per_universe)
{
  std::vector<Fixture> res;
  res.reserve(std::clamp(count, 0, 512));
  const int64_t used = usedChannels(fixture);
  const int universe = fixture.universe;
  int64_t next = address;
  for(int i = 0; i < count; i++)
  {
    const auto [u, a] = fitInUniverse(universe, next, used, channels_per_universe);
    fixture.universe = u;
    fixture.address = a;
    res.push_back(fixture);
    next = int64_t(u - universe) * std::max(channels_per_universe, 0) + a + used
           + std::max(spacing, 0);
  }
  return res;
}
}
}
#endif
