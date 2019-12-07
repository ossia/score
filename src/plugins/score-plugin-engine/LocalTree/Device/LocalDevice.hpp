#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>

#include <nano_observer.hpp>

namespace ossia
{
namespace net
{
class device_base;
class multiplex_protocol;
}
}

namespace Protocols
{
class LocalDevice final : public Device::DeviceInterface
{
public:
  LocalDevice(
      ossia::net::device_base& dev,
      const score::DocumentContext& ctx,
      const Device::DeviceSettings& settings);

  ~LocalDevice() override;

  void setRemoteSettings(const Device::DeviceSettings&);

  ossia::net::device_base* getDevice() const override { return &m_dev; }

private:
  void disconnect() override;
  bool reconnect() override;

  Device::Node refresh() override;

  ossia::net::device_base& m_dev;
  ossia::net::multiplex_protocol* m_proto{};
  using DeviceInterface::refresh;
};
}
