#pragma once

#include <Explorer/DefaultProtocolFactory.hpp>

#include <ossia/protocols/hid/hid_device.hpp>

namespace Protocols
{
class HIDProtocolFactory final : public DefaultProtocolFactory
{
  SCORE_CONCRETE("68e15f6d-0aa3-455a-8442-c77c0931c1f2")

  QString prettyName() const noexcept override;
  QString category() const noexcept override;
  QUrl manual() const noexcept override;

  Device::DeviceEnumerators
  getEnumerators(const score::DocumentContext& ctx) const override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin,
      const score::DocumentContext& ctx) override;

  const Device::DeviceSettings& defaultSettings() const noexcept override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(
      const QVariant& data, const VisitorVariant& visitor) const override;

  bool checkCompatibility(
      const Device::DeviceSettings& a,
      const Device::DeviceSettings& b) const noexcept override;
};

Device::DeviceSettings hidDeviceSettings(const ossia::net::hid_device_info& info);
}
