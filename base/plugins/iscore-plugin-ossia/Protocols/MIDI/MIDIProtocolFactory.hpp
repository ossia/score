#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include "MIDIDevice.hpp"
#include "MIDIProtocolSettingsWidget.hpp"

class MIDIProtocolFactory : public ProtocolFactory
{
        // Implement with OSSIA::Device
        QString prettyName() const override
        {
            return QObject::tr("MIDI");
        }

        const ProtocolFactoryKey& key_impl() const override
        {
            static const ProtocolFactoryKey name{"MIDI"};
            return name;
        }

        DeviceInterface* makeDevice(const iscore::DeviceSettings& settings) override
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

        bool checkCompatibility(
                const iscore::DeviceSettings& a,
                const iscore::DeviceSettings& b) const override
        {
            return a.name != b.name;
        }
};
