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
class SpoutProtocolFactory final : public SharedOutputProtocolFactory
{
  SCORE_CONCRETE("ddf45db7-9eaf-453c-8fc0-86ccdf21677c")

public:
  QString prettyName() const noexcept override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& doc,
      const score::DocumentContext& ctx) override;
  const Device::DeviceSettings& defaultSettings() const noexcept override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;
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

class SpoutSettingsWidget final : public SharedOutputSettingsWidget
{
public:
  SpoutSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
};

}
