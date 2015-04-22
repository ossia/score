#pragma once
#include <DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp>
#include <DeviceExplorer/Protocol/MIDI/MIDIDevice.hpp>
#include <DeviceExplorer/Protocol/MIDI/MIDIProtocolSettingsWidget.hpp>

class MIDIProtocolFactory : public ProtocolFactoryInterface
{
        // Implement with OSSIA::Device
        QString name() const override
        { return "MIDI"; }

        DeviceInterface* makeDevice(const DeviceSettings& settings) override
        {
            return new MIDIDevice{settings};
        }

        ProtocolSettingsWidget* makeSettingsWidget() override
        {
            return new MIDIProtocolSettingsWidget;
        }

        QVariant makeProtocolSpecificSettings(QVariant source) const override
        {
            return makeProtocolSpecificSettings_T<MIDISpecificSettings>(source);
        }
};
