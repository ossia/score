#pragma once

#include <Explorer/DefaultProtocolFactory.hpp>

namespace Protocols
{

class LeapmotionProtocolFactory final : public DefaultProtocolFactory
{
  SCORE_CONCRETE("e75178a6-a0cc-417e-bc14-eb75705cc1f6")

  QString prettyName() const noexcept override;
  QString category() const noexcept override;
  Device::DeviceEnumerator*
  getEnumerator(const score::DocumentContext& ctx) const override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin,
      const score::DocumentContext& ctx) override;

  const Device::DeviceSettings& defaultSettings() const noexcept override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  QVariant
  makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(
      const QVariant& data,
      const VisitorVariant& visitor) const override;

  bool checkCompatibility(
      const Device::DeviceSettings& a,
      const Device::DeviceSettings& b) const noexcept override;
};

}
