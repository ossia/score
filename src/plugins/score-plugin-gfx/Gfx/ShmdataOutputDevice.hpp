#pragma once
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <Gfx/GfxDevice.hpp>

#include <QLineEdit>

namespace Gfx
{
  struct ShmSettings {
    QString path;
    int width{};
    int height{};
    double rate{};
  };

class gfx_protocol_base;
class ShmdataOutputProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("69bb8215-dae2-4ec9-b60c-79f4f4fc2390")
  public:
  QString prettyName() const noexcept override;
  QString category() const noexcept override;
  Device::DeviceEnumerator*
  getEnumerator(const score::DocumentContext& ctx) const override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& doc,
      const score::DocumentContext& ctx) override;
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

  QVariant
  makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(
      const QVariant& data,
      const VisitorVariant& visitor) const override;

  bool checkCompatibility(
      const Device::DeviceSettings& a,
      const Device::DeviceSettings& b) const noexcept override;
};

class ShmdataOutputDevice final : public GfxOutputDevice
{
  W_OBJECT(ShmdataOutputDevice)
public:
  using GfxOutputDevice::GfxOutputDevice;
  ~ShmdataOutputDevice();

private:
  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  gfx_protocol_base* m_protocol{};
  mutable std::unique_ptr<ossia::net::device_base> m_dev;
};

class ShmdataOutputSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  ShmdataOutputSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void setDefaults();
  QLineEdit* m_deviceNameEdit{};
  QLineEdit* m_shmPath{};
  QSpinBox* m_width{};
  QSpinBox* m_height{};
  QSpinBox* m_rate{};
};

}

SCORE_SERIALIZE_DATASTREAM_DECLARE(, Gfx::ShmSettings);
Q_DECLARE_METATYPE(Gfx::ShmSettings)
W_REGISTER_ARGTYPE(Gfx::ShmSettings)
