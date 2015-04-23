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
        virtual void serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor) const = 0;

        template<typename T>
        void serializeProtocolSpecificSettings_T(const QVariant& data, const VisitorVariant& visitor) const
        {
            switch(visitor.identifier)
            {
                case DataStream::type():
                {
                    static_cast<DataStream::Reader*>(visitor.visitor)->readFrom(data.value<T>());
                    break;
                }
                case JSON::type():
                {
                    static_cast<JSON::Reader*>(visitor.visitor)->readFrom(data.value<T>());
                    break;
                }
                default:
                    Q_ASSERT(false);
            }
        }


        // Load
        virtual QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const = 0;

        template<typename T>
        QVariant makeProtocolSpecificSettings_T(const VisitorVariant& visitor) const
        {
            T settings;

            switch(visitor.identifier)
            {
                case DataStream::type():
                {
                    static_cast<DataStream::Writer*>(visitor.visitor)->writeTo(settings);
                    break;
                }
                case JSON::type():
                {
                    static_cast<JSON::Writer*>(visitor.visitor)->writeTo(settings);
                    break;
                }
                default:
                    Q_ASSERT(false);
            }

            return QVariant::fromValue(settings);
        }

};
