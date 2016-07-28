#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>

namespace Ossia
{
namespace Protocols
{
class MinuitProtocolFactory final :
        public Device::ProtocolFactory
{
        ISCORE_CONCRETE_FACTORY("f1b90a2a-b5e2-4cd7-9b75-987df7e25bdc")
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
}
