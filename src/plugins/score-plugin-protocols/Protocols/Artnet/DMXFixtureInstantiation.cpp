#include <ossia/detail/config.hpp>

#if defined(OSSIA_PROTOCOL_ARTNET)
#include "DMXFixtureInstantiation.hpp"

#include <Protocols/Artnet/ArtnetSpecificSettings.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/protocols/artnet/dmx_led_parameter.hpp>
#include <ossia/protocols/artnet/dmx_parameter.hpp>
namespace Protocols
{

namespace
{
struct fixture_setup_visitor
{
  const Artnet::Fixture& fix;
  const Artnet::Channel& chan;
  ossia::net::node_base& fixt_node;
  ossia::net::dmx_buffer& buffer;
  int dmx_channel;
  int operator()(const Artnet::SingleCapability& v) const noexcept
  {
    auto chan_node = fixt_node.create_child(chan.name.toStdString());
    auto chan_param
        = std::make_unique<ossia::net::dmx_parameter>(*chan_node, buffer, dmx_channel);

    auto& node = *chan_node;
    auto& p = *chan_param;

    // FIXME this only works if the channels are joined for now
    int bytes = 1;

    for(auto& name : chan.fineChannels)
    {
      if(ossia::contains(fix.mode.channelNames, name))
      {
        bytes++;
      }
    }
    p.m_bytes = bytes;

    chan_node->set_parameter(std::move(chan_param));
    p.set_default_value(chan.defaultValue);
    p.set_value(chan.defaultValue);

    if(!v.comment.isEmpty())
      ossia::net::set_description(node, v.comment.toStdString());

    return bytes;
  }

  int operator()(const std::vector<Artnet::RangeCapability>& v) const noexcept
  {
    std::vector<std::pair<std::string, uint8_t>> values;
    std::string comment;
    std::string default_preset;

    // Parse all capabilities
    for(auto& capa : v)
    {
      std::string name;
      if(!capa.effectName.isEmpty())
        name = capa.effectName.toStdString();
      else
        name = capa.type.toStdString();

      if(chan.defaultValue >= capa.range.first && chan.defaultValue < capa.range.second)
        default_preset = name;
      values.push_back({name, capa.range.first});
    }

    // Make sure that all values have an unique name
    {
      std::vector<int> counts;
      for(int i = 0; i < std::ssize(values); i++)
      {
        int n = 1;
        for(int j = 0; j < i; j++)
        {
          if(values[j].first == values[i].first)
            n++;
        }
        counts.push_back(n);
      }
      for(int i = 0; i < std::ssize(values); i++)
      {
        if(counts[i] > 1)
          values[i].first += fmt::format(" {}", counts[i]);
      }
    }

    // Write the comment
    for(int i = 0; i < std::ssize(values); i++)
    {
      if(!v[i].comment.isEmpty())
      {
        comment += values[i].first + ": " + v[i].comment.toStdString() + "\n";
      }
    }

    if(values.empty())
      return 0;

    auto chan_node = fixt_node.create_child(chan.name.toStdString());
    auto chan_param
        = std::make_unique<ossia::net::dmx_parameter>(*chan_node, buffer, dmx_channel);

    chan_param->set_default_value(chan.defaultValue);
    chan_param->set_value(chan.defaultValue);
    auto& chan_param_ref = *chan_param;

    if(!comment.empty())
      ossia::net::set_description(*chan_node, comment);

    chan_node->set_parameter(std::move(chan_param));
    {
      auto chan_enumnode = chan_node->create_child("preset");
      auto chan_enumparam = std::make_unique<ossia::net::dmx_enum_parameter>(
          *chan_enumnode, chan_param_ref, values);

      auto& node = *chan_enumnode;
      auto& p = *chan_enumparam;

      if(default_preset.empty())
        default_preset = values.front().first;
      p.set_default_value(default_preset);
      p.set_value(default_preset);

      if(!comment.empty())
        ossia::net::set_description(node, std::move(comment));

      chan_enumnode->set_parameter(std::move(chan_enumparam));
    }
    return 1;
  }
};

struct led_visitor
{
  ossia::net::node_base& fixt_node;
  ossia::net::dmx_buffer& buffer;
  int dmx_channel;

  int operator()(const Artnet::LEDStripLayout& v) const noexcept
  {
    auto chan_param = std::make_unique<ossia::net::dmx_led_parameter>(
        fixt_node, buffer, dmx_channel, v.diodes.size(), v.length);

    fixt_node.set_parameter(std::move(chan_param));
    return v.channels();
  }

  int operator()(const Artnet::LEDPaneLayout& v) const noexcept
  {
    auto chan_param = std::make_unique<ossia::net::dmx_led_parameter>(
        fixt_node, buffer, dmx_channel, v.diodes.size(), v.width * v.height);

    fixt_node.set_parameter(std::move(chan_param));
    return v.channels();
  }

  int operator()(const Artnet::LEDVolumeLayout& v) const noexcept
  {
    auto chan_param = std::make_unique<ossia::net::dmx_led_parameter>(
        fixt_node, buffer, dmx_channel, v.diodes.size(), v.width * v.height * v.depth);

    fixt_node.set_parameter(std::move(chan_param));
    return v.channels();
  }

  int operator()(ossia::monostate) { return 0; }
};

}

void addArtnetFixture(
    ossia::net::generic_device& dev, ossia::net::dmx_buffer& buffer,
    const Artnet::Fixture& fix, int channels_per_universe)
{
  int dmx_channel = 0;
  int size = 0;
  // For each fixture, we'll create a node.
  auto fixt_node = dev.create_child(fix.fixtureName.toStdString());
  if(!fixt_node)
    return;

  // For each channel, a sub-node that goes [0-255] or more depending on bit depth
  for(auto& chan : fix.controls)
  {
    // Get the dmx offset of this channel:
    const int channel_offset
        = ossia::index_in_container(fix.mode.channelNames, chan.name);
    if(channel_offset == -1)
      continue;
    dmx_channel = fix.address + channel_offset;

    // Then for each range-based subchannels, sub-nodes with the relevant domains.
    fixture_setup_visitor vis{fix, chan, *fixt_node, buffer, dmx_channel};

    size = ossia::visit(vis, chan.capabilities);
  }

  if(fix.led)
  {
    dmx_channel = fix.address;
    led_visitor vis{*fixt_node, buffer, dmx_channel};

    size = ossia::visit(vis, fix.led);
  }

  int max = dmx_channel + size;
  int max_universe_count = 1 + max / channels_per_universe;
  if(buffer.universes() < max_universe_count)
    buffer.set_universe_count(max_universe_count);
}

}
#endif
