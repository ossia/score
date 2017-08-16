#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <ossia/dataflow/audio_protocol.hpp>
class QLineEdit;
namespace Dataflow
{
class AudioProtocolFactory final : public Device::ProtocolFactory
{
    ISCORE_CONCRETE("2835e6da-9b55-4029-9802-e1c817acbdc1")
    QString prettyName() const override;

    Device::DeviceInterface* makeDevice(
        const Device::DeviceSettings& settings,
        const iscore::DocumentContext& ctx) override;
    const Device::DeviceSettings& defaultSettings() const override;

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
    AudioDevice(const Device::DeviceSettings& settings, const iscore::DocumentContext& c);

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
    ossia::net::device_base& m_dev;
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

struct AudioSpecificSettings
{
    QString card;
    int rate{44100};
    int bufferSize{64};
};
}
Q_DECLARE_METATYPE(Dataflow::AudioSpecificSettings)
