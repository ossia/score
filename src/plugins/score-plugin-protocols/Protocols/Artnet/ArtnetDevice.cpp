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
static void addArtnetFixture(
    ossia::net::generic_device& dev, ossia::net::dmx_buffer& buffer,
    const Artnet::Fixture& fix)
{
  // For each fixture, we'll create a node.
  auto fixt_node = dev.create_child(fix.fixtureName.toStdString());
  if(!fixt_node)
    return;

  // For each channel, a sub-node that goes [0-255] or more depending on bit depth
  int k = fix.address;
  for(auto& chan : fix.controls)
  {
    auto chan_node = fixt_node->create_child(chan.name.toStdString());
    auto chan_param = std::make_unique<ossia::net::dmx_parameter>(*chan_node, buffer, k);
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

    // Then for each range-based subchannels, sub-nodes with the relevant domains.
    struct chan_visitor
    {
      ossia::net::node_base& node;
      ossia::net::dmx_buffer& buffer;
      int k;
      void operator()(const Artnet::SingleCapability& v) const noexcept
      {
        if(!v.comment.isEmpty())
          ossia::net::set_description(node, v.comment.toStdString());
      }

      void operator()(const std::vector<Artnet::RangeCapability>& v) const noexcept
      {
        for(auto& capa : v)
        {
          std::string name;
          if(!capa.effectName.isEmpty())
            name = capa.effectName.toStdString();
          else
            name = capa.type.toStdString();

          auto cld = node.create_child(name);
          auto cld_p = std::make_unique<ossia::net::dmx_parameter>(
              *cld, buffer, k, capa.range.first, capa.range.second);
          cld_p->set_value(int(capa.range.first));
          cld->set_parameter(std::move(cld_p));

          assert(dynamic_cast<ossia::net::dmx_parameter*>(cld->get_parameter()));
          if(!capa.comment.isEmpty())
            ossia::net::set_description(*cld, capa.comment.toStdString());
        }
      }
    } vis{*chan_node, buffer, k};

    ossia::visit(vis, chan.capabilities);

    k++;
  }
}
}
bool ArtnetDevice::reconnect()
{
  disconnect();

  try
  {
    const auto& set = m_settings.deviceSpecificSettings.value<ArtnetSpecificSettings>();

    ossia::net::dmx_config conf;
    conf.autocreate = set.fixtures.empty();
    conf.frequency = set.rate;
    conf.universe = set.universe;
    conf.multicast = true;

    switch(set.transport)
    {
      case ArtnetSpecificSettings::ArtNet: {
        auto artnet_proto = std::make_unique<ossia::net::artnet_protocol>(m_ctx, conf);
        auto& proto = *artnet_proto;
        auto dev = std::make_unique<ossia::net::generic_device>(
            std::move(artnet_proto), settings().name.toStdString());

        for(auto& fixt : set.fixtures)
        {
          addArtnetFixture(*dev, proto.buffer(), fixt);
        }
        m_dev = std::move(dev);
        break;
      }
      case ArtnetSpecificSettings::E131: {
        ossia::net::socket_configuration sock_conf;
        sock_conf.host = set.host.toStdString();
        sock_conf.port = ossia::net::e131_protocol::default_port;

        auto artnet_proto
            = std::make_unique<ossia::net::e131_protocol>(m_ctx, conf, sock_conf);
        auto& proto = *artnet_proto;
        auto dev = std::make_unique<ossia::net::generic_device>(
            std::move(artnet_proto), settings().name.toStdString());

        for(auto& fixt : set.fixtures)
        {
          addArtnetFixture(*dev, proto.buffer(), fixt);
        }
        m_dev = std::move(dev);
        break;
      }
      case ArtnetSpecificSettings::DMXUSBPRO: {
        ossia::net::serial_configuration sock_conf;

        for(auto& p : QSerialPortInfo::availablePorts())
        {
          if(p.portName() == set.host)
          {
            sock_conf.port = p.systemLocation().toStdString();
            break;
          }
        }

        sock_conf.baud_rate = 115200;

        auto artnet_proto
            = std::make_unique<ossia::net::dmxusbpro_protocol>(m_ctx, conf, sock_conf);
        auto& proto = *artnet_proto;
        auto dev = std::make_unique<ossia::net::generic_device>(
            std::move(artnet_proto), settings().name.toStdString());

        for(auto& fixt : set.fixtures)
        {
          addArtnetFixture(*dev, proto.buffer(), fixt);
        }
        m_dev = std::move(dev);
        break;
      }
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
  OwningDeviceInterface::disconnect();
}
}
#endif
