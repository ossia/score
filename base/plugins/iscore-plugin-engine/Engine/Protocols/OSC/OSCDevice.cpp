#include <QDebug>
#include <QString>
#include <QVariant>
#include <memory>

#include "OSCDevice.hpp"
#include <ossia/network/generic/generic_address.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/osc/osc.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/Protocols/OSC/OSCSpecificSettings.hpp>

namespace Engine
{
namespace Network
{
OSCDevice::OSCDevice(const Device::DeviceSettings& settings)
    : OwningOSSIADevice{settings}
{
  using namespace ossia;
  m_capas.canLearn = true;
  m_capas.hasCallbacks = false;

  reconnect();
}

bool OSCDevice::reconnect()
{
  disconnect();

  try
  {
    auto stgs = settings().deviceSpecificSettings.value<OSCSpecificSettings>();
    std::unique_ptr<ossia::net::protocol_base> ossia_settings
        = std::make_unique<ossia::net::osc_protocol>(
            stgs.host.toStdString(), stgs.inputPort, stgs.outputPort);
    m_dev = std::make_unique<ossia::net::generic_device>(
        std::move(ossia_settings), settings().name.toStdString());
    setLogging_impl(isLogging());
  }
  catch (...)
  {
    ISCORE_TODO;
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
  auto& proto = static_cast<ossia::net::osc_protocol&>(m_dev->getProtocol());
  return proto.getLearningStatus();
}

void OSCDevice::setLearning(bool b)
{
  auto& proto = static_cast<ossia::net::osc_protocol&>(m_dev->getProtocol());
  auto& dev = *m_dev;
  if (b)
  {
    dev.onNodeCreated.connect<OSSIADevice, &OSSIADevice::nodeCreated>(
        (OSSIADevice*)this);
    dev.onNodeRemoving.connect<OSSIADevice, &OSSIADevice::nodeRemoving>(
        (OSSIADevice*)this);
    dev.onNodeRenamed.connect<OSSIADevice, &OSSIADevice::nodeRenamed>(
        (OSSIADevice*)this);
    dev.onAddressCreated.connect<OSSIADevice, &OSSIADevice::addressCreated>(
        (OSSIADevice*)this);
    dev.onAttributeModified.connect<OSSIADevice, &OSSIADevice::addressUpdated>(
        (OSSIADevice*)this);
  }
  else
  {
    dev.onNodeCreated.disconnect<OSSIADevice, &OSSIADevice::nodeCreated>(
        (OSSIADevice*)this);
    dev.onNodeRemoving.disconnect<OSSIADevice, &OSSIADevice::nodeRemoving>(
        (OSSIADevice*)this);
    dev.onNodeRenamed.disconnect<OSSIADevice, &OSSIADevice::nodeRenamed>(
        (OSSIADevice*)this);
    dev.onAddressCreated.disconnect<OSSIADevice, &OSSIADevice::addressCreated>(
        (OSSIADevice*)this);
    dev.onAttributeModified
        .disconnect<OSSIADevice, &OSSIADevice::addressUpdated>(
            (OSSIADevice*)this);
  }

  proto.setLearningStatus(b);
}
}
}
