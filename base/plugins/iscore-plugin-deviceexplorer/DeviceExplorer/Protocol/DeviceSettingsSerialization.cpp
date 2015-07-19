#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "Singletons/SingletonProtocolList.hpp"
#include "ProtocolFactoryInterface.hpp"
#include "DeviceSettings.hpp"
template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::DeviceSettings& n)
{
    m_stream << n.name
             << n.protocol;

    auto prot = SingletonProtocolList::instance().protocol(n.protocol);
    prot->serializeProtocolSpecificSettings(n.deviceSpecificSettings, this->toVariant());

    insertDelimiter();
}
template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::DeviceSettings& n)
{
    m_stream >> n.name
             >> n.protocol;

    auto prot = SingletonProtocolList::instance().protocol(n.protocol);
    n.deviceSpecificSettings = prot->makeProtocolSpecificSettings(this->toVariant());

    checkDelimiter();
}
template<>
void Visitor<Reader<JSONObject>>::readFrom(const iscore::DeviceSettings& n)
{
    m_obj["Name"] = n.name;
    m_obj["Protocol"] = n.protocol;

    auto prot = SingletonProtocolList::instance().protocol(n.protocol);
    prot->serializeProtocolSpecificSettings(n.deviceSpecificSettings, this->toVariant());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(iscore::DeviceSettings& n)
{
    n.name = m_obj["Name"].toString();
    n.protocol = m_obj["Protocol"].toString();

    auto prot = SingletonProtocolList::instance().protocol(n.protocol);
    n.deviceSpecificSettings = prot->makeProtocolSpecificSettings(this->toVariant());
}
