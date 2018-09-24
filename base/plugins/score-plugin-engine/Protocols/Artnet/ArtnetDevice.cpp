
#include "ArtnetDevice.hpp"
#include "ArtnetSpecificSettings.hpp"
#include <ossia/network/artnet/artnet_protocol.hpp>


W_OBJECT_IMPL(Engine::Network::ArtnetDevice)

namespace Engine::Network {

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

ArtnetDevice::~ArtnetDevice()
{
}

bool ArtnetDevice::reconnect()
{
    disconnect();
  
    try
    {
        auto addr =
            std::make_unique<ossia::net::generic_device>(
                std::make_unique<ossia::net::artnet_protocol>(40),  //  40 Hz update rate
                settings().name.toStdString());
        m_dev = std::move(addr);
        deviceChanged(nullptr, m_dev.get());
    }
    catch (...)
    {
      SCORE_TODO;
    }


  return connected();
}

void ArtnetDevice::disconnect()
{
  OwningDeviceInterface::disconnect();
}


}
