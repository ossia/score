#pragma once
#include <DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp>
#include <DeviceExplorer/Protocol/OSC/OSCDevice.hpp>
#include <DeviceExplorer/Protocol/OSC/OSCProtocolSettingsWidget.hpp>

class OSCProtocolFactory : public ProtocolFactoryInterface
{
        // Implement with OSSIA::Device
        QString name() const override
        { return "OSC"; }

        DeviceInterface* makeDevice(const DeviceSettings& settings) override
        {
            return new OSCDevice{settings};
        }

        virtual ProtocolSettingsWidget* makeSettingsWidget() override
        {
            return new OSCProtocolSettingsWidget;
        }

        QVariant makeProtocolSpecificSettings(QVariant source) const override
        {
            return makeProtocolSpecificSettings_T<OSCSpecificSettings>(source);
        }
};
