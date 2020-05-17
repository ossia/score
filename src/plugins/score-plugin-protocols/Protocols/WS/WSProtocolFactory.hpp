#pragma once
#include <Explorer/DefaultProtocolFactory.hpp>

namespace Protocols
{
class WSProtocolFactory final : public DefaultProtocolFactory
{
  SCORE_CONCRETE("59e81303-af24-4559-b33d-1c6f59f0f017")
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
