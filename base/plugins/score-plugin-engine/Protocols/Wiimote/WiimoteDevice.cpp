
#include "WiimoteDevice.hpp"
#include "WiimoteSpecificSettings.hpp"
#include <ossia/network/wiimote/wiimote_protocol.hpp>

W_OBJECT_IMPL(Engine::Network::WiimoteDevice)

namespace Engine::Network {

WiimoteDevice::WiimoteDevice(const Device::DeviceSettings& settings)
    : OwningDeviceInterface{settings}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canSerialize = false;
}

WiimoteDevice::~WiimoteDevice()
{
}

bool WiimoteDevice::reconnect()
{
  disconnect();

  try
  {
    auto stgs
        = settings().deviceSpecificSettings.value<WiimoteSpecificSettings>();

    m_dev = std::make_unique<ossia::net::generic_device>(
        std::make_unique<ossia::net::wiimote_protocol>(
            false),
        settings().name.toStdString());
    deviceChanged(nullptr, m_dev.get());
  }
  catch (...)
  {
    SCORE_TODO;
  }

  return connected();
}

void WiimoteDevice::disconnect()
{
  OwningDeviceInterface::disconnect();
}

}
