#pragma once
#include <DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp>
#include "MIDIDevice.hpp"
#include "MIDIProtocolSettingsWidget.hpp"

class MIDIProtocolFactory : public ProtocolFactory
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

        QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override
        {
            return makeProtocolSpecificSettings_T<MIDISpecificSettings>(visitor);
        }

        void serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor) const override
        {
            serializeProtocolSpecificSettings_T<MIDISpecificSettings>(data, visitor);
        }
};
