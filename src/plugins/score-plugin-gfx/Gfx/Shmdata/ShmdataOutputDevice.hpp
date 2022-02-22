#pragma once
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <Gfx/SharedOutputSettings.hpp>
#include <Gfx/GfxDevice.hpp>

#include <QLineEdit>

namespace Gfx
{

class gfx_protocol_base;
class ShmdataOutputProtocolFactory final : public Gfx::SharedOutputProtocolFactory
{
  SCORE_CONCRETE("69bb8215-dae2-4ec9-b60c-79f4f4fc2390")
  public:
  QString prettyName() const noexcept override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& doc,
      const score::DocumentContext& ctx) override;
  const Device::DeviceSettings& defaultSettings() const noexcept override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;
};

}
