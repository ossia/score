#include <ossia/network/base/device.hpp>
#include <QString>
#include <QVariant>
#include <iscore/document/DocumentContext.hpp>
#include <memory>

#include "LocalDevice.hpp"
#include <ossia/editor/state/state_element.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>
#include <ossia/network/minuit/minuit.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <Engine/OSSIA2iscore.hpp>
#include <Engine/Protocols/Local/LocalSpecificSettings.hpp>
#include <Engine/iscore2OSSIA.hpp>
namespace Engine
{
namespace Network
{
LocalDevice::LocalDevice(
    ossia::net::generic_device& dev,
    const iscore::DocumentContext& ctx,
    const Device::DeviceSettings& settings)
    : OSSIADevice{settings}, m_dev{dev}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canSerialize = false;


  auto& proto = dynamic_cast<ossia::net::multiplex_protocol&>(dev.get_protocol());

  m_proto = &proto;
  setLogging_impl(isLogging());

  setRemoteSettings(settings);

  enableCallbacks();
}

LocalDevice::~LocalDevice()
{
}

void LocalDevice::setRemoteSettings(const Device::DeviceSettings& settings)
{
  if (!m_proto)
    return;

  try
  {
    auto set = settings.deviceSpecificSettings.value<LocalSpecificSettings>();
    if (set.remoteName == "")
    {
      set.remoteName = "i-score-remote";
      set.host = "127.0.0.1";
      set.remotePort = 9999;
      set.localPort = 6666;
    }
    m_proto->expose_to(std::make_unique<ossia::net::minuit_protocol>(
        set.remoteName.toStdString(),
        set.host.toStdString(),
        set.remotePort,
        set.localPort));
  }
  catch (...)
  {
    qDebug() << "LocalDevice: could not expose i-score-remote on port 6666";
  }
}

void LocalDevice::disconnect()
{
  // TODO handle listening ??
  setLogging_impl(false);
}

bool LocalDevice::reconnect()
{
  m_callbacks.clear();
  setRemoteSettings(settings());
  return connected();
}

Device::Node LocalDevice::refresh()
{
  Device::Node iscore_device{settings(), nullptr};

  // Recurse on the children
  const auto& ossia_children = m_dev.get_root_node().children();
  iscore_device.reserve(ossia_children.size());
  for (const auto& node : ossia_children)
  {
    iscore_device.push_back(
        Engine::ossia_to_iscore::ToDeviceExplorer(*node.get()));
  }

  iscore_device.get<Device::DeviceSettings>().name
      = QString::fromStdString(m_dev.get_name());

  return iscore_device;
}
}
}
