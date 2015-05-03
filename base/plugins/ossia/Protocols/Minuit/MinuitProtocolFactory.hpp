#pragma once
#include <DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp>
#include "MinuitDevice.hpp"
#include "MinuitProtocolSettingsWidget.hpp"

class MinuitProtocolFactory : public ProtocolFactoryInterface
{
        // Implement with OSSIA::Device
        QString name() const override
        { return "Minuit"; }

        DeviceInterface* makeDevice(const DeviceSettings& settings) override
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
};
