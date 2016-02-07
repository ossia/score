#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <QString>
#include <QVariant>

namespace Device
{
class DeviceInterface;
class ProtocolSettingsWidget;
}
namespace iscore {
struct DeviceSettings;
}  // namespace iscore
struct VisitorVariant;

namespace Ossia
{
class MIDIProtocolFactory final :
        public Device::ProtocolFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("94a362a1-9411-4ee9-b94d-4bc79b1427cf")

        // Implement with OSSIA::Device
        QString prettyName() const override;

        Device::DeviceInterface* makeDevice(
                const Device::DeviceSettings& settings,
                const iscore::DocumentContext& ctx) override;

        const Device::DeviceSettings& defaultSettings() const override;

        Device::ProtocolSettingsWidget* makeSettingsWidget() override;

        QVariant makeProtocolSpecificSettings(
                const VisitorVariant& visitor) const override;

        void serializeProtocolSpecificSettings(
                const QVariant& data,
                const VisitorVariant& visitor) const override;

        bool checkCompatibility(
                const Device::DeviceSettings& a,
                const Device::DeviceSettings& b) const override;
};
}
