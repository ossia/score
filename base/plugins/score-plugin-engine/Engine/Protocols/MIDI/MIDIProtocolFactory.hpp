#pragma once
#include <Engine/Protocols/DefaultProtocolFactory.hpp>
#include <QString>
#include <QVariant>

namespace Device
{
class DeviceInterface;
class ProtocolSettingsWidget;
}
namespace score
{
struct DeviceSettings;
} // namespace score
struct VisitorVariant;

namespace Engine
{
namespace Network
{
class MIDIProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("94a362a1-9411-4ee9-b94d-4bc79b1427cf")

  // Implement with OSSIA::Device
  QString prettyName() const override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const score::DocumentContext& ctx) override;

  const Device::DeviceSettings& defaultSettings() const override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;
  Device::AddressDialog* makeAddAddressDialog(const Device::DeviceInterface& dev, const score::DocumentContext& ctx, QWidget* parent) override;

  Device::AddressDialog* makeEditAddressDialog(
      const Device::AddressSettings&,
      const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx,
      QWidget*) override;
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
