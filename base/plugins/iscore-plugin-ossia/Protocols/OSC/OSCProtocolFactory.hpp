#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include "OSCDevice.hpp"
#include "OSCProtocolSettingsWidget.hpp"

class OSCProtocolFactory : public ProtocolFactory
{
        // Implement with OSSIA::Device
        QString prettyName() const override
        {
            return QObject::tr("OSC");
        }

        const ProtocolFactoryKey& key_impl() const override
        {
            static const ProtocolFactoryKey name{"OSC"};
            return name;
        }


        DeviceInterface* makeDevice(const iscore::DeviceSettings& settings) override
        {
            return new OSCDevice{settings};
        }

        ProtocolSettingsWidget* makeSettingsWidget() override
        {
            return new OSCProtocolSettingsWidget;
        }

        QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override
        {
            return makeProtocolSpecificSettings_T<OSCSpecificSettings>(visitor);
        }

        void serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor) const override
        {
            serializeProtocolSpecificSettings_T<OSCSpecificSettings>(data, visitor);
        }

        bool checkCompatibility(
                const iscore::DeviceSettings& a,
                const iscore::DeviceSettings& b) const override
        {
            auto a_p = a.deviceSpecificSettings.value<OSCSpecificSettings>();
            auto b_p = b.deviceSpecificSettings.value<OSCSpecificSettings>();
            return a.name != b.name && a_p.inputPort != b_p.inputPort && a_p.outputPort != b_p.outputPort;
        }
};
