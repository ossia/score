#pragma once
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <nano_observer.hpp>

namespace ossia
{
namespace net
{
class generic_device;
class local_protocol;
}
}
namespace Engine
{
namespace Network
{
class LocalDevice final : public OSSIADevice
{
public:
  LocalDevice(
      ossia::net::generic_device& dev,
      const iscore::DocumentContext& ctx,
      const Device::DeviceSettings& settings);

  ~LocalDevice();

  void setRemoteSettings(const Device::DeviceSettings&);

  ossia::net::device_base* getDevice() const override
  {
    return &m_dev;
  }

private:
  void disconnect() override;
  bool reconnect() override;

  Device::Node refresh() override;

  ossia::net::device_base& m_dev;
  ossia::net::local_protocol* m_proto;
  using OSSIADevice::refresh;
};
}
}
