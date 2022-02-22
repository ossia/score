#pragma once

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <ossia/gfx/texture_parameter.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>

#include <QLineEdit>

#include <Gfx/GfxInputDevice.hpp>
#include <Gfx/SharedInputSettings.hpp>
class QComboBox;
namespace Gfx::Spout
{
using InputSettings = Gfx::SharedInputSettings;
class InputFactory final : public SharedInputProtocolFactory
{
  SCORE_CONCRETE("3c995cb6-052b-4c52-a8fd-841b33b81b29")
public:
  QString prettyName() const noexcept override;
  Device::DeviceEnumerator* getEnumerator(const score::DocumentContext& ctx) const override;

  Device::DeviceInterface*
  makeDevice(const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin, const score::DocumentContext& ctx) override;
  const Device::DeviceSettings& defaultSettings() const noexcept override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;
};

class InputSettingsWidget final : public SharedInputSettingsWidget
{
public:
  InputSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
};
}
