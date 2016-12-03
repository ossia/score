#pragma once
#include <Engine/Protocols/OSSIADevice.hpp>

namespace Engine
{
namespace Network
{
class HTTPDevice final : public OwningOSSIADevice, public Nano::Observer
{
public:
  HTTPDevice(const Device::DeviceSettings& settings);

  bool reconnect() override;

  void nodeCreated(const ossia::net::node_base& n)
  {
    OSSIADevice::nodeCreated(n);
  }
  void nodeRemoving(const ossia::net::node_base& n)
  {
    OSSIADevice::nodeRemoving(n);
  }
  void nodeRenamed(const ossia::net::node_base& n, std::string s)
  {
    OSSIADevice::nodeRenamed(n, s);
  }
};
}
}
