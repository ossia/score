#pragma once
#include <Gfx/GfxDevice.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/GfxInputDevice.hpp>

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Video/ExternalInput.hpp>
#include <Video/FrameQueue.hpp>

#include <ossia/gfx/texture_parameter.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <verdigris>

class QComboBox;
class QFormLayout;
class QLineEdit;

namespace Gfx::GPhoto2
{

struct GPhoto2Settings
{
  QString model;
  QString port;
};

class ProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("a7e5e6cc-3e7e-4f92-b5f6-0dca37e64c8a")
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

SCORE_SERIALIZE_DATASTREAM_DECLARE(, Gfx::GPhoto2::GPhoto2Settings);
Q_DECLARE_METATYPE(Gfx::GPhoto2::GPhoto2Settings)
W_REGISTER_ARGTYPE(Gfx::GPhoto2::GPhoto2Settings)
