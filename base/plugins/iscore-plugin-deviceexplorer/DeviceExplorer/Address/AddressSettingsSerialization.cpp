#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "AddressSettings.hpp"
#include "DomainSerialization.hpp"
#include <State/ValueSerialization.hpp>
template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::Domain& n)
{
    m_stream << n.min << n.max << n.values;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::Domain& n)
{
    m_stream >> n.min >> n.max >> n.values;
}

template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::AddressSettings& n)
{
    m_stream << n.name
             << n.value
             << n.domain
             << (int)n.ioType
             << (int)n.clipMode
             << n.unit
             << n.repetitionFilter
             << n.rate
             << n.priority
             << n.tags;

    insertDelimiter();
}
template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::AddressSettings& n)
{
    m_stream >> n.name
            >> n.value
            >> n.domain
            >> (int&)n.ioType
            >> (int&)n.clipMode
            >> n.unit
            >> n.repetitionFilter
            >> n.rate
            >> n.priority
            >> n.tags;

    checkDelimiter();
}
// TODO refactor.
template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::FullAddressSettings& n)
{
    m_stream << n.address
             << n.value
             << n.domain
             << (int)n.ioType
             << (int)n.clipMode
             << n.unit
             << n.repetitionFilter
             << n.rate
             << n.priority
             << n.tags;

    insertDelimiter();
}
template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::FullAddressSettings& n)
{
    m_stream >> n.address
            >> n.value
            >> n.domain
            >> (int&)n.ioType
            >> (int&)n.clipMode
            >> n.unit
            >> n.repetitionFilter
            >> n.rate
            >> n.priority
            >> n.tags;

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
    auto type = n.value.val.typeName();
    if(type)
    {
        m_obj["Type"] = QString::fromStdString(type);
        m_obj["Value"] = ValueToJson(n.value);
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

    if(m_obj.contains("Type"))
    {
        auto valueType = QMetaType::Type(QMetaType::type(m_obj["Type"].toString().toLatin1()));
        n.value = JsonToValue(m_obj["Value"], valueType);
        n.domain = JsonToDomain(m_obj["Domain"].toObject(), valueType);
    }
}
