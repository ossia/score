#pragma once
#include <QLineEdit>

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

namespace Gfx
{
class gfx_protocol_base;
class GfxProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("5a181207-7d40-4ad8-814e-879fcdf8cc31")
  QString prettyName() const noexcept override;
  QString category() const noexcept override;
  Device::DeviceEnumerator* getEnumerator(const score::DocumentContext& ctx) const override;

  Device::DeviceInterface*
  makeDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx) override;
  const Device::DeviceSettings& defaultSettings() const noexcept override;
  Device::AddressDialog* makeAddAddressDialog(
      const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx,
      QWidget* parent) override;
  Device::AddressDialog* makeEditAddressDialog(
      const Device::AddressSettings&,
      const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx,
      QWidget*) override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor)
      const override;

  bool checkCompatibility(const Device::DeviceSettings& a, const Device::DeviceSettings& b)
      const noexcept override;
};

class GfxDevice final : public Device::DeviceInterface
{
  W_OBJECT(GfxDevice)
public:
  GfxDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx);
  ~GfxDevice();

  void addAddress(const Device::FullAddressSettings& settings) override;

  void updateAddress(
      const State::Address& currentAddr,
      const Device::FullAddressSettings& settings) override;
  bool reconnect() override;
  void recreate(const Device::Node& n) override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

private:
  using Device::DeviceInterface::refresh;
  QMimeData* mimeData() const override;
  void setupContextMenu(QMenu&) const override;
  Device::Node refresh() override;
  void disconnect() override;

  void setupNode(ossia::net::node_base&, const ossia::extended_attributes& attr);

  const score::DocumentContext& m_ctx;
  gfx_protocol_base* m_protocol{};
  mutable std::unique_ptr<ossia::net::device_base> m_dev;
};

class GfxSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  GfxSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void setDefaults();
  QLineEdit* m_deviceNameEdit{};
};

}
