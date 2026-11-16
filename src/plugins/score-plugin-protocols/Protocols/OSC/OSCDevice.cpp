// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DeviceLogging.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Protocols/OSC/OSCSpecificSettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>

#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/local/local.hpp>
#include <ossia/network/rate_limiting_protocol.hpp>
#include <ossia/network/resolve_transport.hpp>
#include <ossia/protocols/osc/osc_factory.hpp>
#include <ossia/protocols/oscquery/oscquery_server_asio.hpp>

#include <memory>

namespace Protocols
{

OSCDevice::OSCDevice(
    const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx)
    : OwningDeviceInterface{settings}
    , m_ctx{ctx}
{
  m_capas.canLearn = true;
  m_capas.hasCallbacks = false;
}

void OSCDevice::disconnect()
{
  m_oscproto = nullptr;
  m_zeroconf = {};

  auto old = std::move(m_dev);
  deviceChanged(old.get(), nullptr);
  Device::releaseDevice(*m_ctx, std::move(old));
  Device::OwningDeviceInterface::disconnect();
}

struct osc_protocols
{
  std::unique_ptr<ossia::net::protocol_base> ret;
  ossia::net::multiplex_protocol* multiplex{};
  ossia::oscquery_asio::oscquery_server_protocol_base* oscquery{};
  ossia::net::osc_protocol_base* osc{};
  ossia::net::rate_limiting_protocol* ratelimit{};
};

struct convert_osc_transport_to_server
{
  using ret = std::optional<ossia::net::osc_server_configuration>;
  ret operator()(const ossia::net::udp_configuration& conf)
  {
    if(conf.local)
      return ossia::net::udp_server_configuration{*conf.local};
    return std::nullopt;
  }
  ret operator()(const ossia::net::unix_dgram_configuration& conf)
  {
    if(conf.local)
      return ossia::net::unix_dgram_server_configuration{*conf.local};
    return std::nullopt;
  }
  ret operator()(const ossia::net::unix_stream_configuration& conf)
  {
    return std::nullopt;
  }
  ret operator()(const ossia::net::serial_configuration& conf) { return conf; }
  ret operator()(const ossia::net::tcp_server_configuration& conf) { return conf; }
  ret operator()(const ossia::net::ws_server_configuration& conf) { return conf; }
  ret operator()(const ossia::net::tcp_client_configuration& conf)
  {
    return std::nullopt;
  }
  ret operator()(const ossia::net::ws_client_configuration& conf)
  {
    return std::nullopt;
  }

  ret operator()(const auto& conf) { return conf; }
};
osc_protocols make_osc_protocol(
    const ossia::net::network_context_ptr& m_ctx, const OSCSpecificSettings& stgs)
{
  osc_protocols protos;
  auto resolved_configuration = stgs.configuration;
  ossia::resolve_host_in_transport(resolved_configuration.transport);

  if(stgs.oscquery)
  {
    auto multiplex = std::make_unique<ossia::net::multiplex_protocol>();
    protos.multiplex = multiplex.get();

    if(auto proto = ossia::net::make_osc_protocol(m_ctx, resolved_configuration))
    {
      protos.osc = proto.get();
      multiplex->expose_to(std::move(proto));
    }
    else
      return {};

    std::vector<ossia::net::osc_server_configuration> conf;

    ossia::visit([&](const auto& elt) {
      if(auto t = convert_osc_transport_to_server{}(elt))
        conf.push_back(std::move(*t));
    }, resolved_configuration.transport);

    if(auto proto
       = std::make_unique<ossia::oscquery_asio::oscquery_server_protocol_base>(
           m_ctx, conf, *stgs.oscquery, false))
    {
      protos.oscquery = proto.get();
      multiplex->expose_to(std::move(proto));
    }
    protos.ret = std::move(multiplex);
  }
  else if(auto proto = ossia::net::make_osc_protocol(m_ctx, resolved_configuration))
  {
    protos.osc = proto.get();
    protos.ret = std::move(proto);
  }

  if(!protos.ret || !protos.osc)
    return {};

  if(stgs.rate)
  {
    auto rate_stgs = *stgs.rate;
    if(stgs.configuration.bundle_strategy
       == ossia::net::osc_protocol_configuration::ALWAYS_BUNDLE)
    {
      rate_stgs.bundle = true;
    }
    auto rl = std::make_unique<ossia::net::rate_limiting_protocol>(
        rate_stgs, std::move(protos.ret));
    protos.ratelimit = rl.get();
    protos.ret = std::move(rl);
  }
  return protos;
}

bool OSCDevice::reconnect()
{
  disconnect();

  try
  {
    const OSCSpecificSettings& stgs
        = settings().deviceSpecificSettings.value<OSCSpecificSettings>();
    const auto& name = settings().name.toStdString();
    auto protos = make_osc_protocol(m_ctx, stgs);
    if(protos.ret)
    {
      m_dev = std::make_unique<ossia::net::generic_device>(std::move(protos.ret), name);
      m_oscproto = protos.osc;

      if(m_dev)
      {
        setup_zeroconf(stgs, name);
      }
      deviceChanged(nullptr, m_dev.get());
      setLogging_impl(Device::get_cur_logging(isLogging()));
    }
    else
    {
      qDebug() << "Could not create OSC protocol";
    }
  }
  catch(std::exception& e)
  {
    qDebug() << "OSC Protocol error: " << e.what();
  }
  catch(...)
  {
    SCORE_TODO;
  }

  return connected();
}

void OSCDevice::recreate(const Device::Node& n)
{
  for(auto& child : n)
  {
    addNode(child);
  }
}

bool OSCDevice::isLearning() const
{
  if(auto proto = m_oscproto)
    return proto->learning();
  return false;
}

void OSCDevice::setLearning(bool b)
{
  if(!m_dev)
    return;
  if(!m_oscproto)
    return;
  auto& proto = *m_oscproto;
  auto& dev = *m_dev;
  if(b)
  {
    dev.on_node_created.connect<&DeviceInterface::nodeCreated>((DeviceInterface*)this);
    dev.on_node_removing.connect<&DeviceInterface::nodeRemoving>((DeviceInterface*)this);
    dev.on_node_renamed.connect<&DeviceInterface::nodeRenamed>((DeviceInterface*)this);
    dev.on_parameter_created.connect<&DeviceInterface::addressCreated>(
        (DeviceInterface*)this);
    dev.on_attribute_modified.connect<&DeviceInterface::addressUpdated>(
        (DeviceInterface*)this);
  }
  else
  {
    dev.on_node_created.disconnect<&DeviceInterface::nodeCreated>(
        (DeviceInterface*)this);
    dev.on_node_removing.disconnect<&DeviceInterface::nodeRemoving>(
        (DeviceInterface*)this);
    dev.on_node_renamed.disconnect<&DeviceInterface::nodeRenamed>(
        (DeviceInterface*)this);
    dev.on_parameter_created.disconnect<&DeviceInterface::addressCreated>(
        (DeviceInterface*)this);
    dev.on_attribute_modified.disconnect<&DeviceInterface::addressUpdated>(
        (DeviceInterface*)this);
  }

  proto.set_learning(b);
}

void OSCDevice::setup_zeroconf(const OSCSpecificSettings& stgs, const std::string& name)
{
  if(stgs.bonjour)
  {
    if(auto udp = ossia_variant_alias::get_if<ossia::net::udp_configuration>(
           &stgs.configuration.transport))
    {
      m_zeroconf
          = ossia::net::make_zeroconf_server(name, "_osc._udp", "", udp->local->port, 0);
    }
    else if(
        auto tcp = ossia_variant_alias::get_if<ossia::net::tcp_client_configuration>(
            &stgs.configuration.transport))
    {
      m_zeroconf = ossia::net::make_zeroconf_server(name, "_osc._tcp", "", tcp->port, 0);
    }
    else if(
        auto ws = ossia_variant_alias::get_if<ossia::net::ws_server_configuration>(
            &stgs.configuration.transport))
    {
      m_zeroconf = ossia::net::make_zeroconf_server(name, "_osc._ws", "", ws->port, 0);
    }
  }
}
}
