#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "AddressSettings.hpp"
#include "DomainSerialization.hpp"
#include <State/ValueSerialization.hpp>


template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::AddressSettingsCommon& n)
{
    m_stream << n.value
             << n.domain
             << n.ioType
             << n.clipMode
             << n.unit
             << n.repetitionFilter
             << n.rate
             << n.priority
             << n.tags;
}


template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::AddressSettingsCommon& n)
{
    m_stream >> n.value
             >> n.domain
             >> n.ioType
             >> n.clipMode
             >> n.unit
             >> n.repetitionFilter
             >> n.rate
             >> n.priority
             >> n.tags;
}


template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::AddressSettings& n)
{
    readFrom(static_cast<const iscore::AddressSettingsCommon&>(n));
    m_stream << n.name;

    insertDelimiter();
}
template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::AddressSettings& n)
{
    writeTo(static_cast<iscore::AddressSettingsCommon&>(n));
    m_stream >> n.name;

    checkDelimiter();
}
template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::FullAddressSettings& n)
{
    readFrom(static_cast<const iscore::AddressSettingsCommon&>(n));
    m_stream << n.address;

    insertDelimiter();
}
template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::FullAddressSettings& n)
{
    writeTo(static_cast<iscore::AddressSettingsCommon&>(n));
    m_stream >> n.address;

    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const iscore::AddressSettings& n)
{
    m_obj["Name"] = n.name;

    // Metadata
    m_obj["ioType"] = iscore::IOTypeStringMap()[n.ioType];
    m_obj["ClipMode"] = iscore::ClipModeStringMap()[n.clipMode];
    m_obj["Unit"] = n.unit;

    m_obj["RepetitionFilter"] = n.repetitionFilter;
    m_obj["RefreshRate"] = n.rate;

    m_obj["Priority"] = n.priority;

    QJsonArray arr;
    for(auto& str : n.tags)
        arr.append(str);
    m_obj["Tags"] = arr;

    // Value, domain and type
    readFrom(n.value);
    auto type = n.value.val.typeName();
    if(type)
    {
        m_obj["Domain"] = DomainToJson(n.domain);
    }
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(iscore::AddressSettings& n)
{
    n.name = m_obj["Name"].toString();

    n.ioType = iscore::IOTypeStringMap().key(m_obj["ioType"].toString());
    n.clipMode = iscore::ClipModeStringMap().key(m_obj["ClipMode"].toString());
    n.unit = m_obj["Unit"].toString();

    n.repetitionFilter = m_obj["RepetitionFilter"].toBool();
    n.rate = m_obj["RefreshRate"].toInt();

    n.priority = m_obj["Priority"].toInt();

    auto arr = m_obj["Tags"].toArray();
    for(auto&& elt : arr)
        n.tags.append(elt.toString());

    writeTo(n.value);
    // TODO doesn't handle multi-type variants.
    if(m_obj.contains("Type"))
    {
        auto valueType = static_cast<QMetaType::Type>(QMetaType::type(m_obj["Type"].toString().toUtf8()));
        n.domain = JsonToDomain(m_obj["Domain"].toObject(), valueType);
    }
}
