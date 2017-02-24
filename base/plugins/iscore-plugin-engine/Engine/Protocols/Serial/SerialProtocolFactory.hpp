#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>

namespace Engine
{
namespace Network
{
class SerialProtocolFactory final : public Device::ProtocolFactory
{
  ISCORE_CONCRETE("50c48ef3-8e60-400e-9a51-2ab535ad87eb")
  // Implement with OSSIA::Device
  QString prettyName() const override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const iscore::DocumentContext& ctx) override;
  const Device::DeviceSettings& defaultSettings() const override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  QVariant
  makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(
      const QVariant& data, const VisitorVariant& visitor) const override;

  bool checkCompatibility(
      const Device::DeviceSettings& a,
      const Device::DeviceSettings& b) const override;
};
}
}
