#include <ossia/detail/config.hpp>

#include <QDebug>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include "ArtnetDevice.hpp"
#include "ArtnetSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/network/generic/generic_device.hpp>
#include <ossia/protocols/artnet/artnet_protocol.hpp>
#include <ossia/protocols/artnet/dmx_parameter.hpp>
#include <ossia/protocols/artnet/dmxusbpro_protocol.hpp>
#include <ossia/protocols/artnet/e131_protocol.hpp>

#include <QSerialPortInfo>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Protocols::ArtnetDevice)

namespace Protocols
{

ArtnetDevice::ArtnetDevice(
    const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx)
    : OwningDeviceInterface{settings}
    , m_ctx{ctx}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canSerialize = false;
}

ArtnetDevice::~ArtnetDevice() { }

namespace
{
struct fixture_setup_visitor
{
  const Artnet::Fixture& fix;
  const Artnet::Channel& chan;
  ossia::net::node_base& fixt_node;
  ossia::net::dmx_buffer& buffer;
  int dmx_channel;
  void operator()(const Artnet::SingleCapability& v) const noexcept
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
  }

  void operator()(const std::vector<Artnet::RangeCapability>& v) const noexcept
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
      return;

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
  }

  void operator()(const Artnet::LEDStripLayout& v) const noexcept { SCORE_ABORT; }
  void operator()(const Artnet::LEDPaneLayout& v) const noexcept { SCORE_ABORT; }
  void operator()(const Artnet::LEDVolumeLayout& v) const noexcept { SCORE_ABORT; }
};

static void addArtnetFixture(
    ossia::net::generic_device& dev, ossia::net::dmx_buffer& buffer,
    const Artnet::Fixture& fix)
{
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
    const int dmx_channel = fix.address + channel_offset;

    // Then for each range-based subchannels, sub-nodes with the relevant domains.
    fixture_setup_visitor vis{fix, chan, *fixt_node, buffer, dmx_channel};

    ossia::visit(vis, chan.capabilities);
  }
}
}
bool ArtnetDevice::reconnect()
{
  disconnect();

  try
  {
    const auto& set = m_settings.deviceSpecificSettings.value<ArtnetSpecificSettings>();

    // Convert the settings to the ossia format
    ossia::net::dmx_config conf;
    conf.autocreate = ossia::net::dmx_config::no_auto;
    if(set.fixtures.empty())
    {
      if(set.transport == ArtnetSpecificSettings::ArtNet_MultiUniverse
         || set.transport == ArtnetSpecificSettings::E131_MultiUniverse)
      {
        conf.autocreate = ossia::net::dmx_config::just_universes;
      }
      else if(set.transport == ArtnetSpecificSettings::ArtNet)
      {
        conf.autocreate = ossia::net::dmx_config::channel_index;
      }
      else
      {
        conf.autocreate = ossia::net::dmx_config::just_index;
      }
    }
    conf.frequency = set.rate;
    conf.universe = set.universe;
    conf.multicast = true;
    conf.mode = set.mode == ArtnetSpecificSettings::Source
                    ? ossia::net::dmx_config::source
                    : ossia::net::dmx_config::sink;

    // Create the protocol
    std::unique_ptr<ossia::net::dmx_protocol_base> artnet_proto;
    switch(set.transport)
    {
      case ArtnetSpecificSettings::ArtNet:
      case ArtnetSpecificSettings::ArtNetV2:
      case ArtnetSpecificSettings::ArtNet_MultiUniverse: {
        auto host = set.host.toStdString();
        if(host.empty())
          host = "0.0.0.0";

        if(set.mode == ArtnetSpecificSettings::Source)
          artnet_proto
              = std::make_unique<ossia::net::artnet_protocol>(m_ctx, conf, host);
        else
          artnet_proto
              = std::make_unique<ossia::net::artnet_input_protocol>(m_ctx, conf, host);
        break;
      }
      case ArtnetSpecificSettings::E131:
      case ArtnetSpecificSettings::E131_MultiUniverse: {
        auto host = set.host.toStdString();
        if(host.empty())
          host = "0.0.0.0";

        if(set.mode == ArtnetSpecificSettings::Source)
        {
          ossia::net::outbound_socket_configuration sock_conf;
          sock_conf.host = host;
          sock_conf.port = ossia::net::e131_protocol::default_port;

          artnet_proto
              = std::make_unique<ossia::net::e131_protocol>(m_ctx, conf, sock_conf);
        }
        else
        {
          ossia::net::inbound_socket_configuration sock_conf;
          sock_conf.bind = host;
          sock_conf.port = ossia::net::e131_protocol::default_port;

          artnet_proto = std::make_unique<ossia::net::e131_input_protocol>(
              m_ctx, conf, sock_conf);
        }

        break;
      }
      case ArtnetSpecificSettings::DMXUSBPRO:
      case ArtnetSpecificSettings::DMXUSBPRO_Mk2:
      case ArtnetSpecificSettings::OpenDMX_USB: {
        ossia::net::serial_configuration sock_conf;

        for(auto& p : QSerialPortInfo::availablePorts())
        {
          if(p.portName() == set.host)
          {
            sock_conf.port = p.systemLocation().toStdString();
            break;
          }
        }
        if(sock_conf.port.empty())
          sock_conf.port = set.host.toStdString();

        int version = -1;
        switch(set.transport)
        {
          case ArtnetSpecificSettings::DMXUSBPRO:
            version = 1;
            sock_conf.baud_rate = 115200;
            sock_conf.stop_bits = ossia::net::serial_configuration::two;
            break;

          case ArtnetSpecificSettings::DMXUSBPRO_Mk2:
            version = 2;
            sock_conf.baud_rate = 115200;
            sock_conf.stop_bits = ossia::net::serial_configuration::two;
            break;

          case ArtnetSpecificSettings::OpenDMX_USB:
            version = 3;
            sock_conf.baud_rate = 250000;
            sock_conf.stop_bits = ossia::net::serial_configuration::two;
            break;

          default:
            break;
        }

        artnet_proto = std::make_unique<ossia::net::dmxusbpro_protocol>(
            m_ctx, conf, sock_conf, version);
        break;
      }
    }

    // Create the device
    if(artnet_proto)
    {
      auto& proto = *artnet_proto;
      auto dev = std::make_unique<ossia::net::generic_device>(
          std::move(artnet_proto), settings().name.toStdString());

      if(!set.fixtures.empty())
        SCORE_ASSERT(proto.buffer().size() == 1);
      for(auto& fixt : set.fixtures)
      {
        addArtnetFixture(*dev, proto.buffer()[0], fixt);
      }

      if(set.mode == ArtnetSpecificSettings::Sink)
        if(auto p = dynamic_cast<ossia::net::dmx_input_protocol_base*>(&proto))
          p->create_channel_map();
      m_dev = std::move(dev);
    }
    deviceChanged(nullptr, m_dev.get());
  }
  catch(const std::runtime_error& e)
  {
    qDebug() << "ArtNet error: " << e.what();
  }
  catch(...)
  {
    qDebug() << "ArtNet error";
  }

  return connected();
}

void ArtnetDevice::disconnect()
{
  if(m_owned)
  {
    if(m_capas.hasCallbacks)
      disableCallbacks();

    m_callbacks.clear();
    deviceChanged(m_dev.get(), nullptr);
    Device::releaseDevice(*this->m_ctx, std::move(m_dev));
  }
}
}
#endif
