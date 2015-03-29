#pragma once
#include <DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp>
#include <DeviceExplorer/Protocol/Minuit/MinuitDevice.hpp>

class MinuitProtocolFactory : public ProtocolFactoryInterface
{
        // Implement with OSSIA::Device
        QString name() const override
        { return "Minuit"; }

        DeviceInterface* makeDevice(const DeviceSettings& settings) override
        {
            return new MinuitDevice(settings.deviceSpecificSettings.value<MinuitSpecificSettings>());
        }
};
