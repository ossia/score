#pragma once
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Gfx/GfxDevice.hpp>
#include <Gfx/SharedOutputSettings.hpp>

#include <QLineEdit>

namespace Gfx
{

class gfx_protocol_base;
class Sh4ltOutputProtocolFactory final : public Gfx::SharedOutputProtocolFactory
{
  SCORE_CONCRETE("41e367e1-fc36-40b2-b8c4-8aecd5dfd4fc")
public:
  QString prettyName() const noexcept override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& doc,
      const score::DocumentContext& ctx) override;
  const Device::DeviceSettings& defaultSettings() const noexcept override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;
};

}
