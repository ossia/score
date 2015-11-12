#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>

class MIDIProtocolFactory : public ProtocolFactory
{
        // Implement with OSSIA::Device
        QString prettyName() const override;

        const ProtocolFactoryKey& key_impl() const override;

        DeviceInterface* makeDevice(
                const iscore::DeviceSettings& settings) override;

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
