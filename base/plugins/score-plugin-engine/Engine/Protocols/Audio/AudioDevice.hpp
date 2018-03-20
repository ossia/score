#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <ossia/audio/audio_protocol.hpp>
class QLineEdit;
namespace Dataflow
{
class AudioProtocolFactory final : public Device::ProtocolFactory
{
    SCORE_CONCRETE("2835e6da-9b55-4029-9802-e1c817acbdc1")
    QString prettyName() const override;

    Device::DeviceInterface* makeDevice(
        const Device::DeviceSettings& settings,
        const score::DocumentContext& ctx) override;
    const Device::DeviceSettings& defaultSettings() const override;
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

    QVariant
    makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

    void serializeProtocolSpecificSettings(
        const QVariant& data, const VisitorVariant& visitor) const override;

    bool checkCompatibility(
        const Device::DeviceSettings& a,
        const Device::DeviceSettings& b) const override;
};

class AudioDevice final : public Engine::Network::OSSIADevice
{
    Q_OBJECT
  public:
    AudioDevice(const Device::DeviceSettings& settings);

    void addAddress(const Device::FullAddressSettings& settings) override;
    bool reconnect() override;
    void recreate(const Device::Node& n) override;
    ossia::net::device_base* getDevice() const override
    {
      return &m_dev;
    }

  private:
    using Engine::Network::OSSIADevice::refresh;
    Device::Node refresh() override;
    void disconnect() override;
    ossia::audio_protocol* m_protocol{};
    mutable ossia::net::generic_device m_dev;
};


class AudioSettingsWidget final
    : public Device::ProtocolSettingsWidget
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
