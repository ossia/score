#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <QString>
#include <QVariant>

#include <Device/Protocol/ProtocolFactoryKey.hpp>
#include <iscore_lib_device_export.h>

class DeviceInterface;
class ProtocolSettingsWidget;
namespace iscore {
struct DeviceSettings;
}  // namespace iscore
struct VisitorVariant;

class ISCORE_LIB_DEVICE_EXPORT ProtocolFactory : public iscore::GenericFactoryInterface<ProtocolFactoryKey>
{
        ISCORE_FACTORY_DECL("Protocol")

    public:
            using factory_key_type = ProtocolFactoryKey;
        virtual ~ProtocolFactory();

        virtual QString prettyName() const = 0;

        virtual DeviceInterface* makeDevice(
                const iscore::DeviceSettings& settings,
                const iscore::DocumentContext& ctx) = 0;
        virtual ProtocolSettingsWidget* makeSettingsWidget() = 0;
        virtual const iscore::DeviceSettings& defaultSettings() const = 0;

        // Save
        virtual void serializeProtocolSpecificSettings(
                const QVariant& data,
                const VisitorVariant& visitor) const = 0;

        template<typename T>
        void serializeProtocolSpecificSettings_T(
                const QVariant& data,
                const VisitorVariant& visitor) const
        { serialize_dyn(visitor, data.value<T>()); }


        // Load
        virtual QVariant makeProtocolSpecificSettings(
                const VisitorVariant& visitor) const = 0;

        template<typename T>
        QVariant makeProtocolSpecificSettings_T(
                const VisitorVariant& vis) const
        { return QVariant::fromValue(deserialize_dyn<T>(vis)); }


        // Returns true if the two devicesettings can coexist at the same time.
        virtual bool checkCompatibility(
                const iscore::DeviceSettings& a,
                const iscore::DeviceSettings& b) const = 0;
};
