// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/network/base/device.hpp>
#include <QString>
#include <QVariant>
#include <score/document/DocumentContext.hpp>
#include <memory>

#include "LocalDevice.hpp"
#include <ossia/editor/state/state_element.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>
#include <ossia-config.hpp>
#if defined(OSSIA_PROTOCOL_OSCQUERY)
#include <ossia/network/oscquery/oscquery_server.hpp>
#endif
#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <Engine/OSSIA2score.hpp>
#include <Engine/Protocols/Local/LocalSpecificSettings.hpp>
#include <Engine/score2OSSIA.hpp>
#include <Explorer/DeviceList.hpp>
namespace Engine
{
namespace Network
{
LocalDevice::LocalDevice(
    ossia::net::generic_device& dev,
    const score::DocumentContext& ctx,
    const Device::DeviceSettings& settings)
    : OSSIADevice{settings}, m_dev{dev}
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

  setRemoteSettings(settings);

  enableCallbacks();
}

LocalDevice::~LocalDevice()
{
}

void LocalDevice::setRemoteSettings(const Device::DeviceSettings& settings)
{
  m_dev.set_name(settings.name.toStdString());

  if (!m_proto)
    return;

#if defined(OSSIA_PROTOCOL_OSCQUERY)
  try
  {
    m_proto->clear();

    auto set = settings.deviceSpecificSettings.value<LocalSpecificSettings>();
      set.wsPort = 9999;
      set.oscPort = 6666;

    m_proto->expose_to(std::make_unique<ossia::oscquery::oscquery_server_protocol>(set.oscPort, set.wsPort));
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
}
