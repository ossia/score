#pragma once
#include <DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp>
#include <DeviceExplorer/Protocol/Minuit/MinuitDevice.hpp>
#include <DeviceExplorer/Protocol/Minuit/MinuitProtocolSettingsWidget.hpp>

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

        QVariant makeProtocolSpecificSettings(QVariant source) const override
        {
            return makeProtocolSpecificSettings_T<MinuitSpecificSettings>(source);
        }
};
