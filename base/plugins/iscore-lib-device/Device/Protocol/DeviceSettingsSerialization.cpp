#include <Device/Protocol/ProtocolList.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QDebug>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVariant>

#include <Device/Protocol/ProtocolFactoryKey.hpp>
#include "DeviceSettings.hpp"
#include "ProtocolFactoryInterface.hpp"
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/plugins/customfactory/StringFactoryKeySerialization.hpp>

template <typename T> class Reader;
template <typename T> class Writer;

template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const Device::DeviceSettings& n)
{
    m_stream << n.name
             << n.protocol;

    // TODO try to see if this pattern is refactorable with the similar thing
    // usef for CurveSegmentData.

    auto& pl = context.components.factory<Device::DynamicProtocolList>();
    auto prot = pl.list().get(n.protocol);
    if(prot)
    {
        prot->serializeProtocolSpecificSettings(n.deviceSpecificSettings, this->toVariant());
    }
    else
    {
        qDebug() << "Warning: could not serialize device " << n.name;
    }

    insertDelimiter();
}

template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Writer<DataStream>>::writeTo(Device::DeviceSettings& n)
{
    m_stream >> n.name
             >> n.protocol;

    auto& pl = context.components.factory<Device::DynamicProtocolList>();
    auto prot = pl.list().get(n.protocol);
    if(prot)
    {
        n.deviceSpecificSettings = prot->makeProtocolSpecificSettings(this->toVariant());
    }
    else
    {
        qDebug() << "Warning: could not load device " << n.name;
    }

    checkDelimiter();
}
template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Reader<JSONObject>>::readFrom(const Device::DeviceSettings& n)
{
    m_obj["Name"] = n.name;
    m_obj["Protocol"] = toJsonValue(n.protocol);

    auto& pl = context.components.factory<Device::DynamicProtocolList>();
    auto prot = pl.list().get(n.protocol);
    if(prot)
    {
        prot->serializeProtocolSpecificSettings(n.deviceSpecificSettings, this->toVariant());
    }
    else
    {
        qDebug() << "Warning: could not serialize device " << n.name;
    }
}

template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Writer<JSONObject>>::writeTo(Device::DeviceSettings& n)
{
    n.name = m_obj["Name"].toString();
    n.protocol = fromJsonValue<Device::ProtocolFactoryKey>(m_obj["Protocol"]);

    auto& pl = context.components.factory<Device::DynamicProtocolList>();
    auto prot = pl.list().get(n.protocol);
    if(prot)
    {
        n.deviceSpecificSettings = prot->makeProtocolSpecificSettings(this->toVariant());
    }
    else
    {
        qDebug() << "Warning: could not load device " << n.name;
    }
}
