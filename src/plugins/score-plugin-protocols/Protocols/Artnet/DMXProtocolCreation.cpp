#include <ossia/detail/config.hpp>

#if defined(OSSIA_PROTOCOL_ARTNET)
#include "DMXProtocolCreation.hpp"

#include <ossia/protocols/artnet/artnet_protocol.hpp>
#include <ossia/protocols/artnet/dmx_led_parameter.hpp>
#include <ossia/protocols/artnet/dmx_parameter.hpp>
#include <ossia/protocols/artnet/dmxusbpro_protocol.hpp>
#include <ossia/protocols/artnet/e131_protocol.hpp>

#include <QNetworkInterface>
#include <QSerialPortInfo>

namespace Protocols
{

std::unique_ptr<ossia::net::dmx_protocol_base> instantiateDMXProtocol(
    const ossia::net::network_context_ptr& ctx, const ArtnetSpecificSettings& set)
{
  // Convert the settings to the ossia format
  ossia::net::dmx_config conf;
  conf.autocreate = ossia::net::dmx_config::no_auto;
  if(set.fixtures.empty())
  {
    if(set.universe_count > 1)
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
  conf.start_universe = set.start_universe;
  conf.universe_count = set.universe_count;
  conf.multicast = set.multicast;
  conf.channels_per_universe = set.channels_per_universe;
  conf.mode = set.mode == ArtnetSpecificSettings::Source ? ossia::net::dmx_config::source
                                                         : ossia::net::dmx_config::sink;

  // Create the protocol
  std::unique_ptr<ossia::net::dmx_protocol_base> artnet_proto;

  switch(set.transport)
  {
    case ArtnetSpecificSettings::ArtNet:
    case ArtnetSpecificSettings::ArtNetV2: {
      auto host = set.host.toStdString();
      if(host.empty())
        host = "0.0.0.0";

      if(set.mode == ArtnetSpecificSettings::Source)
      {
        bool good = false;
        for(auto iface : QNetworkInterface::allInterfaces())
        {
          for(const auto& entry : iface.addressEntries())
          {
            if(entry.ip().protocol() == QHostAddress::IPv4Protocol)
              if(host == entry.ip().toString())
              {
                host = entry.broadcast().toString().toStdString();
                good = true;
                break;
              }
          }
          if(good)
            break;
        }

        ossia::net::outbound_socket_configuration sock_conf;
        sock_conf.host = host;
        sock_conf.port = 6454;
        artnet_proto
            = std::make_unique<ossia::net::artnet_protocol>(ctx, conf, sock_conf);
      }
      else
      {

        ossia::net::inbound_socket_configuration sock_conf;
        sock_conf.bind = host;
        sock_conf.port = 6454;
        artnet_proto
            = std::make_unique<ossia::net::artnet_input_protocol>(ctx, conf, sock_conf);
      }
      break;
    }
    case ArtnetSpecificSettings::E131: {
      auto host = set.host.toStdString();
      if(host.empty())
        host = "0.0.0.0";

      if(set.mode == ArtnetSpecificSettings::Source)
      {
        ossia::net::outbound_socket_configuration sock_conf;
        sock_conf.host = host;
        sock_conf.port = ossia::net::e131_protocol::default_port;

        artnet_proto = std::make_unique<ossia::net::e131_protocol>(ctx, conf, sock_conf);
      }
      else
      {
        ossia::net::inbound_socket_configuration sock_conf;
        sock_conf.bind = host;
        sock_conf.port = ossia::net::e131_protocol::default_port;

        artnet_proto
            = std::make_unique<ossia::net::e131_input_protocol>(ctx, conf, sock_conf);
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
          ctx, conf, sock_conf, version);
      break;
    }
  }

  return artnet_proto;
}
}
#endif
