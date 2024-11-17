#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_EVDEV)
#include <Explorer/DefaultProtocolFactory.hpp>

namespace Protocols
{

class EvdevProtocolFactory final : public DefaultProtocolFactory
{
  SCORE_CONCRETE("cc093ece-7de2-4459-b17e-507c1f3cc52b")

  QString prettyName() const noexcept override;
  QString category() const noexcept override;
  QUrl manual() const noexcept override;

  Device::DeviceEnumerators
  getEnumerators(const score::DocumentContext& ctx) const override;

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
