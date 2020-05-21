#pragma once
#include <Explorer/DefaultProtocolFactory.hpp>

namespace Protocols
{
class OSCQueryProtocolFactory final : public DefaultProtocolFactory
{
  SCORE_CONCRETE("1187fefe-0468-49d1-9eb7-92d7f4e11f6f")
  // Implement with OSSIA::Device
  QString prettyName() const noexcept override;
  QString category() const noexcept override;
  int visualPriority() const noexcept override;
  Device::DeviceEnumerator* getEnumerator(const score::DocumentContext& ctx) const override;

  Device::DeviceInterface*
  makeDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx) override;
  const Device::DeviceSettings& defaultSettings() const noexcept override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor)
      const override;

  bool checkCompatibility(const Device::DeviceSettings& a, const Device::DeviceSettings& b)
      const noexcept override;
};
}
