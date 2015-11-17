#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <Device/Protocol/SingletonProtocolList.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>
#include "ProtocolFactoryInterface.hpp"
#include "DeviceSettings.hpp"
#include <core/application/ApplicationComponents.hpp>
#include <Device/Protocol/ProtocolList.hpp>
template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::DeviceSettings& n)
{
    m_stream << n.name
             << n.protocol;

    // TODO try to see if this pattern is refactorable with the similar thing
    // usef for CurveSegmentData.

    auto& pl = context.components.factory<DynamicProtocolList>();
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
void Visitor<Writer<DataStream>>::writeTo(iscore::DeviceSettings& n)
{
    m_stream >> n.name
             >> n.protocol;

    auto& pl = context.components.factory<DynamicProtocolList>();
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
void Visitor<Reader<JSONObject>>::readFrom(const iscore::DeviceSettings& n)
{
    m_obj["Name"] = n.name;
    m_obj["Protocol"] = toJsonValue(n.protocol);

    auto& pl = context.components.factory<DynamicProtocolList>();
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
void Visitor<Writer<JSONObject>>::writeTo(iscore::DeviceSettings& n)
{
    n.name = m_obj["Name"].toString();
    n.protocol = fromJsonValue<ProtocolFactoryKey>(m_obj["Protocol"]);

    auto& pl = context.components.factory<DynamicProtocolList>();
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
