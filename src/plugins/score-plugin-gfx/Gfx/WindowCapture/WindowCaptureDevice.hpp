#pragma once
#include <Gfx/GfxDevice.hpp>
#include <Gfx/GfxInputDevice.hpp>
#include <Gfx/WindowCapture/WindowCaptureNode.hpp>

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <verdigris>

class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QFormLayout;
class QLineEdit;

namespace Gfx::WindowCapture
{

class WindowCaptureProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("a7c1e3f0-5d2b-4e8a-9f6c-1b3d5e7a9c0f")
public:
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

}

SCORE_SERIALIZE_DATASTREAM_DECLARE(, Gfx::WindowCapture::WindowCaptureSettings);
Q_DECLARE_METATYPE(Gfx::WindowCapture::WindowCaptureSettings)
W_REGISTER_ARGTYPE(Gfx::WindowCapture::WindowCaptureSettings)
