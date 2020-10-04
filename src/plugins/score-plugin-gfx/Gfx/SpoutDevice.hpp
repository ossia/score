#pragma once
#include <QLineEdit>
#include <Gfx/GfxDevice.hpp>

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

namespace Gfx
{
class gfx_protocol_base;
class SpoutProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("ddf45db7-9eaf-453c-8fc0-86ccdf21677c")
  QString prettyName() const noexcept override;
  QString category() const noexcept override;
  Device::DeviceEnumerator* getEnumerator(const score::DocumentContext& ctx) const override;

  Device::DeviceInterface*
  makeDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx) override;
  const Device::DeviceSettings& defaultSettings() const noexcept override;
  Device::AddressDialog* makeAddAddressDialog(
      const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx,
      QWidget* parent) override;
  Device::AddressDialog* makeEditAddressDialog(
      const Device::AddressSettings&,
      const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx,
      QWidget*) override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor)
      const override;

  bool checkCompatibility(const Device::DeviceSettings& a, const Device::DeviceSettings& b)
      const noexcept override;
};

class SpoutDevice final : public GfxOutputDevice
{
  W_OBJECT(SpoutDevice)
public:
    using GfxOutputDevice::GfxOutputDevice;
  ~SpoutDevice();

private:
  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  gfx_protocol_base* m_protocol{};
  mutable std::unique_ptr<ossia::net::device_base> m_dev;
};

class SpoutSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  SpoutSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void setDefaults();
  QLineEdit* m_deviceNameEdit{};
};

}
