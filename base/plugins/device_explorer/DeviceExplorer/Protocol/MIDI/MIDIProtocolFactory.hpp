#pragma once
#include <DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp>
#include <DeviceExplorer/Protocol/MIDI/MIDIDevice.hpp>

class MIDIProtocolFactory : public ProtocolFactoryInterface
{
        // Implement with OSSIA::Device
        QString name() const override
        { return "MIDI"; }

        DeviceInterface* makeDevice(const DeviceSettings& settings) override
        {
            return new MIDIDevice(settings.deviceSpecificSettings.value<MIDISpecificSettings>());
        }
};
