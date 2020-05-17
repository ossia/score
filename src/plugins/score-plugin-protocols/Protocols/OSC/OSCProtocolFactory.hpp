#pragma once
#include <Explorer/DefaultProtocolFactory.hpp>

namespace Protocols
{
class OSCProtocolFactory final : public DefaultProtocolFactory
{
  SCORE_CONCRETE("9a42de4b-f6eb-4bca-9564-01b975f601b9")
  // Implement with OSSIA::Device
  QString prettyName() const override;

  Device::DeviceInterface*
  makeDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx) override;
  const Device::DeviceSettings& defaultSettings() const override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor)
      const override;

  bool checkCompatibility(const Device::DeviceSettings& a, const Device::DeviceSettings& b)
      const override;
};
}
