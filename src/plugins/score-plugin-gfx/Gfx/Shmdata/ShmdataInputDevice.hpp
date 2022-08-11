#pragma once

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <Gfx/GfxInputDevice.hpp>
#include <Gfx/SharedInputSettings.hpp>

#include <ossia/gfx/texture_parameter.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>

#include <QLineEdit>

class QComboBox;
namespace Gfx::Shmdata
{
class InputFactory final : public SharedInputProtocolFactory
{
  SCORE_CONCRETE("8062b2e5-c589-41f1-8977-96c5ba782f95")
public:
  QString prettyName() const noexcept override;
  Device::DeviceEnumerator*
  getEnumerator(const score::DocumentContext& ctx) const override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin,
      const score::DocumentContext& ctx) override;
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
