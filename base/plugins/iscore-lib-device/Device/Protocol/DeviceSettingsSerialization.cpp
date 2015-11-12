#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <Device/Protocol/SingletonProtocolList.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>
#include "ProtocolFactoryInterface.hpp"
#include "DeviceSettings.hpp"
template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::DeviceSettings& n)
{
    m_stream << n.name
             << n.protocol;

    // TODO try to see if this pattern is refactorable with the similar thing
    // usef for CurveSegmentData.
    if(!n.protocol.empty())
    {
        auto prot = SingletonProtocolList::instance().get(n.protocol);
        ISCORE_ASSERT(prot);
        prot->serializeProtocolSpecificSettings(n.deviceSpecificSettings, this->toVariant());
    }

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::DeviceSettings& n)
{
    m_stream >> n.name
             >> n.protocol;

    if(!n.protocol.empty())
    {
        auto prot = SingletonProtocolList::instance().get(n.protocol);
        ISCORE_ASSERT(prot);
        n.deviceSpecificSettings = prot->makeProtocolSpecificSettings(this->toVariant());
    }

    checkDelimiter();
}
template<>
void Visitor<Reader<JSONObject>>::readFrom(const iscore::DeviceSettings& n)
{
    m_obj["Name"] = n.name;
    m_obj["Protocol"] = QString::fromStdString(n.protocol);

    if(!n.protocol.empty())
    {
        auto prot = SingletonProtocolList::instance().get(n.protocol);
        ISCORE_ASSERT(prot);
        prot->serializeProtocolSpecificSettings(n.deviceSpecificSettings, this->toVariant());
    }
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(iscore::DeviceSettings& n)
{
    n.name = m_obj["Name"].toString();
    n.protocol = m_obj["Protocol"].toString().toStdString();

    if(!n.protocol.empty())
    {
        auto prot = SingletonProtocolList::instance().get(n.protocol);
        ISCORE_ASSERT(prot);
        n.deviceSpecificSettings = prot->makeProtocolSpecificSettings(this->toVariant());
    }
}
