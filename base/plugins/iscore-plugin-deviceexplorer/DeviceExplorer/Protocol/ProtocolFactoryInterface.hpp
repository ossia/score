#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <DeviceExplorer/Protocol/ProtocolSettingsWidget.hpp>
#include <QString>
#include <QVariant>
#include <DeviceExplorer/Protocol/DeviceSettings.hpp>

#include <iscore/serialization/VisitorCommon.hpp>

class DeviceInterface;
class ProtocolFactory : public iscore::FactoryInterface
{
    public:
        static QString factoryName();
        virtual QString name() const = 0;

        virtual DeviceInterface* makeDevice(const iscore::DeviceSettings& settings) = 0;
        virtual ProtocolSettingsWidget* makeSettingsWidget() = 0;

        // Save
        virtual void serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor) const = 0;

        template<typename T>
        void serializeProtocolSpecificSettings_T(const QVariant& data, const VisitorVariant& visitor) const
        {
            serialize_dyn(visitor, data.value<T>());
        }


        // Load
        virtual QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const = 0;

        template<typename T>
        QVariant makeProtocolSpecificSettings_T(const VisitorVariant& vis) const
        {
            return QVariant::fromValue(deserialize_dyn<T>(vis));
        }


        // Returns true if the two devicesettings can coexist at the same time.
        virtual bool checkCompatibility(const iscore::DeviceSettings& a,
                                        const iscore::DeviceSettings& b) = 0;
};



