// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LocalDevice.hpp"

#include <Explorer/DeviceLogging.hpp>

#include <LocalTree/Device/LocalSpecificSettings.hpp>

#include <score/document/DocumentContext.hpp>
#if defined(OSSIA_PROTOCOL_OSCQUERY)
#include <ossia/network/oscquery/oscquery_server.hpp>
#endif
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/local/local.hpp>

#include <ossia-qt/invoke.hpp>

#include <QApplication>
#include <QDebug>

#include <ossia-config.hpp>

namespace Protocols
{
LocalDevice::LocalDevice(
    ossia::net::device_base& dev, const score::DocumentContext& ctx,
    const Device::DeviceSettings& settings)
    : DeviceInterface{settings}
    , m_ctx{ctx}
    , m_dev{dev}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canRemoveNode = false;
  m_capas.canSerialize = false;

  auto& proto = dynamic_cast<ossia::net::multiplex_protocol&>(dev.get_protocol());

  m_proto = &proto;
  setLogging_impl(Device::get_cur_logging(isLogging()));
  // FIXME instead make the logging a property and bind to it.

  enableCallbacks();
}

LocalDevice::~LocalDevice() { }

static void
exposeZeroconf(std::string name, LocalSpecificSettings set, QPointer<LocalDevice> self)
{
  ossia::net::zeroconf_server ws;
  ossia::net::zeroconf_server osc;
  try
  {
    ws = ossia::net::make_zeroconf_server(name, "_oscjson._tcp", "", set.wsPort, 0);
  }
  catch(const std::exception& e)
  {
    ossia::logger().error("LocalDevice::createZeroconf: {}", e.what());
  }
  catch(...)
  {
    ossia::logger().error("LocalDevice::createZeroconf: error.");
  }

  try
  {
    osc = ossia::net::make_zeroconf_server(name, "_osc._udp", "", set.oscPort, 0);
  }
  catch(const std::exception& e)
  {
    ossia::logger().error("LocalDevice::createZeroconf: {}", e.what());
  }
  catch(...)
  {
    ossia::logger().error("LocalDevice::createZeroconf: error.");
  }

  if(!self)
    return;

  ossia::qt::run_async(qApp, [self, ws = std::move(ws), osc = std::move(osc)]() mutable {
    if(!self)
      return;
    if(auto proto = self->oscqProto())
      proto->set_zeroconf_servers(std::move(ws), std::move(osc));
  });
}

void LocalDevice::init()
{
  m_dev.set_name(m_settings.name.toStdString());

  if(!m_proto)
    return;

#if defined(OSSIA_PROTOCOL_OSCQUERY) && !defined(__EMSCRIPTEN__)
  try
  {
    m_oscqProto = nullptr;
    m_proto->clear();

    auto set = m_settings.deviceSpecificSettings.value<LocalSpecificSettings>();
    set.wsPort = 9999;
    set.oscPort = 6666;

    m_oscqProto = new ossia::oscquery::oscquery_server_protocol(set.oscPort, set.wsPort);
    m_oscqProto->disable_zeroconf();
    m_proto->expose_to(
        std::unique_ptr<ossia::oscquery::oscquery_server_protocol>(m_oscqProto));

    if(auto plug = m_ctx.findPlugin<Explorer::DeviceDocumentPlugin>())
    {
      QPointer<LocalDevice> self;
      plug->networkContext()->context.post(
          [=, name = m_dev.get_name()] { exposeZeroconf(name, set, self); });
    }
    else
    {
      exposeZeroconf(m_dev.get_name(), set, this);
    }
  }
  catch(...)
  {
    qDebug() << "LocalDevice: could not expose score on port 6666";
  }
#endif
}

void LocalDevice::disconnect()
{
  // TODO handle listening ??
  setLogging_impl(Device::get_cur_logging(false));
}

bool LocalDevice::reconnect()
{
  m_callbacks.clear();
  init();
  return connected();
}

Device::Node LocalDevice::refresh()
{
  return simple_refresh();
}
}
