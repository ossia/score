#include <ossia/detail/config.hpp>

#include <QDebug>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include "ArtnetDevice.hpp"
#include "ArtnetSpecificSettings.hpp"

#include <ossia/network/artnet/artnet_protocol.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Protocols::ArtnetDevice)

namespace Protocols
{

ArtnetDevice::ArtnetDevice(const Device::DeviceSettings& settings)
    : OwningDeviceInterface{settings}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canSerialize = false;
}

ArtnetDevice::~ArtnetDevice() { }

bool ArtnetDevice::reconnect()
{
  disconnect();

  try
  {
    auto addr = std::make_unique<ossia::net::generic_device>(
        std::make_unique<ossia::net::artnet_protocol>(
            20), //  20 Hz update rate : TODO add control on widget
        settings().name.toStdString());
    m_dev = std::move(addr);
    deviceChanged(nullptr, m_dev.get());
  }
  catch (const std::runtime_error& e)
  {
    qDebug() << "ArtNet error: " << e.what();
  }
  catch (...)
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
