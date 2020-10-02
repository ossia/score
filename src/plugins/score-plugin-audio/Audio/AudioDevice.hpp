#pragma once
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <ossia/network/generic/generic_device.hpp>

#include <score_plugin_audio_export.h>

#include <verdigris>

namespace ossia
{
class audio_protocol;
}
class QLineEdit;
namespace Dataflow
{
class AudioProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("2835e6da-9b55-4029-9802-e1c817acbdc1")
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

class SCORE_PLUGIN_AUDIO_EXPORT AudioDevice final : public Device::DeviceInterface
{
  W_OBJECT(AudioDevice)
public:
  AudioDevice(const Device::DeviceSettings& settings);
  ~AudioDevice();

  void addAddress(const Device::FullAddressSettings& settings) override;

  void updateAddress(
      const State::Address& currentAddr,
      const Device::FullAddressSettings& settings) override;
  bool reconnect() override;
  void recreate(const Device::Node& n) override;
  ossia::net::device_base* getDevice() const override { return &m_dev; }
  ossia::audio_protocol* getProtocol() const { return m_protocol; }

  void changed() E_SIGNAL(SCORE_PLUGIN_AUDIO_EXPORT, changed);

private:
  using Device::DeviceInterface::refresh;
  void setupNode(ossia::net::node_base&, const ossia::extended_attributes& attr);
  Device::Node refresh() override;
  void disconnect() override;
  ossia::audio_protocol* m_protocol{};
  mutable ossia::net::generic_device m_dev;
};

class AudioSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  AudioSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void setDefaults();
  QLineEdit* m_deviceNameEdit{};
};
}
