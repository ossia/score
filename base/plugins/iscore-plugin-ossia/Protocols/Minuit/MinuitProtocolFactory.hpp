#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include "MinuitDevice.hpp"
#include "MinuitProtocolSettingsWidget.hpp"

class MinuitProtocolFactory : public ProtocolFactory
{
        // Implement with OSSIA::Device
        QString name() const override
        { return "Minuit"; }

        DeviceInterface* makeDevice(const iscore::DeviceSettings& settings) override
        {
            return new MinuitDevice{settings};
        }

        virtual ProtocolSettingsWidget* makeSettingsWidget() override
        {
            return new MinuitProtocolSettingsWidget;
        }

        QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override
        {
            return makeProtocolSpecificSettings_T<MinuitSpecificSettings>(visitor);
        }

        void serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor) const override
        {
            serializeProtocolSpecificSettings_T<MinuitSpecificSettings>(data, visitor);
        }

        bool checkCompatibility(
                const iscore::DeviceSettings& a,
                const iscore::DeviceSettings& b) const override
        {
            // TODO we should check also for other devices.  Devices should have a "open ports" method that
            // returns the ports they use.
            auto a_p = a.deviceSpecificSettings.value<MinuitSpecificSettings>();
            auto b_p = b.deviceSpecificSettings.value<MinuitSpecificSettings>();
            return a.name != b.name && a_p.inputPort != b_p.inputPort && a_p.outputPort != b_p.outputPort;
        }
};
