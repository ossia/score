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

        // Save
        virtual void serializeProtocolSpecificSettings(const QVariant& data, AbstractVisitor* visitor) const = 0;

        template<typename T>
        void serializeProtocolSpecificSettings_T(const QVariant& data, AbstractVisitor* visitor) const
        {
            if(auto ser = dynamic_cast<Visitor<Reader<DataStream>>*>(visitor))
            {
                ser->readFrom(data.value<T>());
            }
            else if(auto ser = dynamic_cast<Visitor<Reader<JSON>>*>(visitor))
            {
                ser->readFrom(data.value<T>());
            }
            else
            {
                Q_ASSERT(false);
            }
        }


        // Load
        virtual QVariant makeProtocolSpecificSettings(AbstractVisitor* visitor) const = 0;

        template<typename T>
        QVariant makeProtocolSpecificSettings_T(AbstractVisitor* visitor) const
        {
            if(auto deser = dynamic_cast<Visitor<Writer<DataStream>>*>(visitor))
            {
                T settings;
                deser->writeTo(settings);
                return QVariant::fromValue(settings);
            }
            else if(auto deser = dynamic_cast<Visitor<Writer<JSON>>*>(visitor))
            {
                T settings;
                deser->writeTo(settings);
                return QVariant::fromValue(settings);
            }

            Q_ASSERT(false);
        }

};
