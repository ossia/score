#pragma once

#include <Explorer/DefaultProtocolFactory.hpp>

namespace Protocols
{

class JoystickProtocolFactory final : public DefaultProtocolFactory
{
  SCORE_CONCRETE("2b9c9f9d-f0fa-41a0-8e7a-0eedd4c48b35")

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
