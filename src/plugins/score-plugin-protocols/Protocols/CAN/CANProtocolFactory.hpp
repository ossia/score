#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_CAN)
#include <Explorer/DefaultProtocolFactory.hpp>

namespace Protocols
{
class CANProtocolFactory final : public DefaultProtocolFactory
{
  SCORE_CONCRETE("2492941c-18ee-4f96-ac3d-c3d42c0bb649")

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

  // defaultSettings() hands back a reference, but the interface it names is a
  // property of the machine *now*: an adapter plugged in after score started
  // must show up. Recomputed on each call into this, rather than cached in a
  // function-local static like the other protocols do.
  mutable Device::DeviceSettings m_defaultSettings;
};
}
#endif
