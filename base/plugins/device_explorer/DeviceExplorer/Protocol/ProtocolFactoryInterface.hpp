#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <DeviceExplorer/Protocol/ProtocolSettingsWidget.hpp>
#include <QString>
#include <QVariant>
#include <DeviceExplorer/Protocol/DeviceSettings.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

class DeviceInterface;
class ProtocolFactoryInterface : public iscore::FactoryInterface
{
    public:
        virtual QString name() const = 0;

        virtual DeviceInterface* makeDevice(const DeviceSettings& settings) = 0;
        virtual ProtocolSettingsWidget* makeSettingsWidget() = 0;
        // Make widget
        // Make device

        virtual QVariant makeProtocolSpecificSettings(QVariant source) const = 0;

        template<typename T>
        QVariant makeProtocolSpecificSettings_T(QVariant source) const override
        {
            if(source.canConvert<Visitor<Writer<DataStream>>*>())
            {
                auto deser = source.value<Visitor<Writer<DataStream>>*>();
                T settings;
                // TODO deser->writeTo(settings);
                return QVariant::fromValue(settings);
            }
            else if(source.canConvert<Visitor<Writer<JSON>>*>())
            {
                auto deser = source.value<Visitor<Writer<DataStream>>*>();
                T settings;
                // TODO deser->writeTo(settings);
                return QVariant::fromValue(settings);
            }

            Q_ASSERT(false);
        }

};
