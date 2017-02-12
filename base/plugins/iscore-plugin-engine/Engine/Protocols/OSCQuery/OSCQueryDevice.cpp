#include <QString>
#include <QVariant>
#include <memory>

#include "OSCQueryDevice.hpp"
#include <ossia/network/generic/generic_address.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/oscquery/oscquery_mirror.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/Protocols/OSCQuery/OSCQuerySpecificSettings.hpp>

namespace Engine
{
namespace Network
{
OSCQueryDevice::OSCQueryDevice(const Device::DeviceSettings& settings)
    : OwningOSSIADevice{settings}
{
  m_capas.canRefreshTree = true;

  reconnect();
}

bool OSCQueryDevice::reconnect()
{
  disconnect();

  try
  {
    auto stgs
        = settings().deviceSpecificSettings.value<OSCQuerySpecificSettings>();

    std::unique_ptr<ossia::net::protocol_base> ossia_settings
        = std::make_unique<ossia::oscquery::oscquery_mirror_protocol>(
            stgs.host.toStdString());

    m_dev = std::make_unique<ossia::net::generic_device>(
        std::move(ossia_settings), settings().name.toStdString());

    setLogging_impl(isLogging());

    m_dev->onNodeCreated.connect<OSSIADevice, &OSSIADevice::nodeCreated>(this);
    m_dev->onNodeRemoving.connect<OSSIADevice, &OSSIADevice::nodeRemoving>(this);
    m_dev->onNodeRenamed.connect<OSSIADevice, &OSSIADevice::nodeRenamed>(this);
    m_dev->onAddressCreated.connect<OSSIADevice, &OSSIADevice::addressCreated>(
        this);
    m_dev->onAttributeModified.connect<OSSIADevice, &OSSIADevice::addressUpdated>(
        this);
  }
  catch (std::exception& e)
  {
    qDebug() << "Could not connect: " << e.what();
  }
  catch (...)
  {
    // TODO save the reason of the non-connection.
  }

  return connected();
}

void OSCQueryDevice::recreate(const Device::Node& n)
{
  for(auto& child : n)
  {
    addNode(child);
  }
}

}
}
