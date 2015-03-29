#pragma once
#include <DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp>
#include <DeviceExplorer/Protocol/OSC/OSCDevice.hpp>

class OSCProtocolFactory : public ProtocolFactoryInterface
{
        // Implement with OSSIA::Device
        QString name() const override
        { return "OSC"; }

        DeviceInterface* makeDevice(const DeviceSettings& settings) override
        {
            return new OSCDevice(settings.deviceSpecificSettings.value<OSCSpecificSettings>());
        }
};
