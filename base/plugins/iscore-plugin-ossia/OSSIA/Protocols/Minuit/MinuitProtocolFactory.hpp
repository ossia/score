#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <QString>
#include <QVariant>

#include <Device/Protocol/ProtocolFactoryKey.hpp>

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
class MinuitProtocolFactory final :
        public Device::ProtocolFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("d305c12e-31f0-46e3-8c9b-3b8744092fc4")
        // Implement with OSSIA::Device
        QString prettyName() const override;

        Device::DeviceInterface* makeDevice(
                const Device::DeviceSettings& settings,
                const iscore::DocumentContext& ctx) override;
        const Device::DeviceSettings& defaultSettings() const override;


        Device::ProtocolSettingsWidget* makeSettingsWidget() override;

        QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

        void serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor) const override;

        bool checkCompatibility(
                const Device::DeviceSettings& a,
                const Device::DeviceSettings& b) const override;
};
}
