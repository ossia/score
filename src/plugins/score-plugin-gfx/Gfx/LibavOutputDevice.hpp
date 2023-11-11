#pragma once
#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV
#include <Gfx/GfxDevice.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/GfxInputDevice.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <Gfx/LibavOutputSettings.hpp>

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
class LibavOutputProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("c8272026-5c7d-4835-84a3-74bef137761f")
  QString prettyName() const noexcept override;
  QString category() const noexcept override;
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

SCORE_SERIALIZE_DATASTREAM_DECLARE(, Gfx::LibavOutputSettings);
Q_DECLARE_METATYPE(Gfx::LibavOutputSettings)
W_REGISTER_ARGTYPE(Gfx::LibavOutputSettings)
#endif
