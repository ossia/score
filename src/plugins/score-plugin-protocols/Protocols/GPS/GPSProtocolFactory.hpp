#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_GPS)
#include <Explorer/DefaultProtocolFactory.hpp>

namespace Protocols
{

class GPSProtocolFactory final : public DefaultProtocolFactory
{
  SCORE_CONCRETE("63cc4e26-b15b-41f9-860f-2b410c86e177")

  QString prettyName() const noexcept override;
  QString category() const noexcept override;
  QUrl manual() const noexcept override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plug,
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
}
#endif
