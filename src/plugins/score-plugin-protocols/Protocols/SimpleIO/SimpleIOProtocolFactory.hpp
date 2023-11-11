#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SIMPLEIO)
#include <Explorer/DefaultProtocolFactory.hpp>

namespace Protocols
{

class SimpleIOProtocolFactory final : public DefaultProtocolFactory
{
  SCORE_CONCRETE("f0c58b8c-c1ea-4dee-9496-245c90a3e5ad")

  QString prettyName() const noexcept override;
  QString category() const noexcept override;

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
