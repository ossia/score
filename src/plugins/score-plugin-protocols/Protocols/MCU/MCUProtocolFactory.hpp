#pragma once
#include <Explorer/DefaultProtocolFactory.hpp>

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

namespace Protocols
{
class MCUProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("4f0f482b-f6ba-4968-9f57-86948c5ca17b")
public:
  // Implement with OSSIA::Device
  Device::ProtocolFactory::Flags flags() const noexcept override;
  QString prettyName() const noexcept override;
  QString category() const noexcept override;
  QUrl manual() const noexcept override;

  Device::DeviceEnumerators
  getEnumerators(const score::DocumentContext& ctx) const override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin,
      const score::DocumentContext& ctx) override;

  const Device::DeviceSettings& defaultSettings() const noexcept override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;
  Device::AddressDialog* makeAddAddressDialog(
      const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
      QWidget* parent) override;

  Device::AddressDialog* makeEditAddressDialog(
      const Device::AddressSettings&, const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx, QWidget*) override;
  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(
      const QVariant& data, const VisitorVariant& visitor) const override;

  bool checkCompatibility(
      const Device::DeviceSettings& a,
      const Device::DeviceSettings& b) const noexcept override;
};
}
