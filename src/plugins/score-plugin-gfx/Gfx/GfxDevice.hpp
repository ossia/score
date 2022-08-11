#pragma once
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <QLineEdit>

#include <score_plugin_gfx_export.h>
namespace Gfx
{
class gfx_protocol_base;
class SCORE_PLUGIN_GFX_EXPORT GfxInputDevice : public Device::DeviceInterface
{
public:
  GfxInputDevice(
      const Device::DeviceSettings& settings, const score::DocumentContext& ctx);
  virtual ~GfxInputDevice();

protected:
  const score::DocumentContext& m_ctx;

  void addAddress(const Device::FullAddressSettings& settings) override;
  void recreate(const Device::Node& n) override;

  using Device::DeviceInterface::refresh;
  QMimeData* mimeData() const override;
  Device::Node refresh() override;
  void disconnect() override;

  void setupNode(ossia::net::node_base&, const ossia::extended_attributes& attr);
};

class SCORE_PLUGIN_GFX_EXPORT GfxOutputDevice : public Device::DeviceInterface
{
public:
  GfxOutputDevice(
      const Device::DeviceSettings& settings, const score::DocumentContext& ctx);
  virtual ~GfxOutputDevice();

protected:
  const score::DocumentContext& m_ctx;

  void addAddress(const Device::FullAddressSettings& settings) override;
  void recreate(const Device::Node& n) override;

  using Device::DeviceInterface::refresh;
  QMimeData* mimeData() const override;
  Device::Node refresh() override;
  void disconnect() override;

  void setupNode(ossia::net::node_base&, const ossia::extended_attributes& attr);
};

}
