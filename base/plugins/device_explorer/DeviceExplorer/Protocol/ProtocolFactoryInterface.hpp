#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <QString>
#include <QVariant>
#include <DeviceExplorer/Protocol/DeviceSettings.hpp>
class DeviceInterface;
class ProtocolFactoryInterface : public iscore::FactoryInterface
{
    public:
        virtual QString name() const = 0;

        virtual DeviceInterface* makeDevice(const DeviceSettings& settings) = 0;
        // Make widget
        // Make device
};
