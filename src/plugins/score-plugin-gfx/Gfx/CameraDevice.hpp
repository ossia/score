#pragma once
#include <Gfx/GfxDevice.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/GfxInputDevice.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <Video/CameraInput.hpp>

#include <ossia/gfx/texture_parameter.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>

#include <QLineEdit>

class QComboBox;

// Score part

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

namespace Gfx
{
struct CameraSettings
{
  QString input;
  QString device;
  QSize size{};
  double fps{};
};

class CameraProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("d615690b-f2e2-447b-b70e-a800552db69c")
  QString prettyName() const noexcept override;
  QString category() const noexcept override;
  Device::DeviceEnumerator*
  getEnumerator(const score::DocumentContext& ctx) const override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin,
      const score::DocumentContext& ctx) override;
  const Device::DeviceSettings& defaultSettings() const noexcept override;
  Device::AddressDialog* makeAddAddressDialog(
      const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
      QWidget* parent) override;
  Device::AddressDialog* makeEditAddressDialog(
      const Device::AddressSettings&, const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx, QWidget*) override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(
      const QVariant& data, const VisitorVariant& visitor) const override;

  bool checkCompatibility(
      const Device::DeviceSettings& a,
      const Device::DeviceSettings& b) const noexcept override;
};

class CameraDevice final : public GfxInputDevice
{
  W_OBJECT(CameraDevice)
public:
  using GfxInputDevice::GfxInputDevice;
  ~CameraDevice();

private:
  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  Gfx::video_texture_input_protocol* m_protocol{};
  mutable std::unique_ptr<Gfx::video_texture_input_device> m_dev;
};

class CameraSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  CameraSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void setDefaults();
  QLineEdit* m_deviceNameEdit{};
  Device::DeviceSettings m_settings;
};

}

SCORE_SERIALIZE_DATASTREAM_DECLARE(, Gfx::CameraSettings);
Q_DECLARE_METATYPE(Gfx::CameraSettings)
W_REGISTER_ARGTYPE(Gfx::CameraSettings)
