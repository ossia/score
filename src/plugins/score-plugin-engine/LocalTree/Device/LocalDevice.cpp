// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LocalDevice.hpp"

#include <Explorer/DeviceLogging.hpp>

#include <score/document/DocumentContext.hpp>

#include <LocalTree/Device/LocalSpecificSettings.hpp>
#if defined(OSSIA_PROTOCOL_OSCQUERY)
#include <ossia/network/oscquery/oscquery_server.hpp>
#endif
#include <ossia/network/base/device.hpp>
#include <ossia/network/local/local.hpp>

#include <QDebug>

#include <ossia-config.hpp>

namespace Protocols
{
LocalDevice::LocalDevice(
    ossia::net::device_base& dev,
    const score::DocumentContext& ctx,
    const Device::DeviceSettings& settings)
    : DeviceInterface{settings}, m_dev{dev}
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

  setRemoteSettings(settings);

  enableCallbacks();
}

LocalDevice::~LocalDevice() { }

void LocalDevice::setRemoteSettings(const Device::DeviceSettings& settings)
{
  m_dev.set_name(settings.name.toStdString());

  if (!m_proto)
    return;

#if defined(OSSIA_PROTOCOL_OSCQUERY) && !defined(__EMSCRIPTEN__)
  try
  {
    m_proto->clear();

    auto set = settings.deviceSpecificSettings.value<LocalSpecificSettings>();
    set.wsPort = 9999;
    set.oscPort = 6666;

    try
    {
      m_proto->expose_to(
          std::make_unique<ossia::oscquery::oscquery_server_protocol>(set.oscPort, set.wsPort));
    }
    catch (...)
    {
    }
  }
  catch (...)
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
  setRemoteSettings(settings());
  return connected();
}

Device::Node LocalDevice::refresh()
{
  return simple_refresh();
}
}
