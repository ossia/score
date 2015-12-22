#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <QString>
#include <QVariant>

#include <Device/Protocol/ProtocolFactoryKey.hpp>

class DeviceInterface;
class ProtocolSettingsWidget;
namespace iscore {
struct DeviceSettings;
}  // namespace iscore
struct VisitorVariant;

class MIDIProtocolFactory : public ProtocolFactory
{
        // Implement with OSSIA::Device
        QString prettyName() const override;

        const ProtocolFactoryKey& key_impl() const override;

        DeviceInterface* makeDevice(
                const iscore::DeviceSettings& settings,
                const iscore::DocumentContext& ctx) override;

        const iscore::DeviceSettings& defaultSettings() const override;

        ProtocolSettingsWidget* makeSettingsWidget() override;

        QVariant makeProtocolSpecificSettings(
                const VisitorVariant& visitor) const override;

        void serializeProtocolSpecificSettings(
                const QVariant& data,
                const VisitorVariant& visitor) const override;

        bool checkCompatibility(
                const iscore::DeviceSettings& a,
                const iscore::DeviceSettings& b) const override;
};
