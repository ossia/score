#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <State/ValueSerialization.hpp>

#include "DomainSerialization.hpp"

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

QJsonObject DomainToJson(const iscore::Domain& d)
{
    QJsonObject obj;
    if(d.min.val.isValid())
        obj["Min"] = ValueToJson(d.min);
    if(d.max.val.isValid())
        obj["Max"] = ValueToJson(d.max);

    QJsonArray arr;
    for(auto& val : d.values)
        arr.append(ValueToJson(val));
    obj["Values"] = arr;

    return obj;
}

iscore::Domain JsonToDomain(const QJsonObject& obj, QMetaType::Type t)
{
    iscore::Domain d;
    if(obj.contains("Min"))
    {
        d.min = JsonToValue(obj["Min"], t);
    }
    if(obj.contains("Max"))
    {
        d.max = JsonToValue(obj["Max"], t);
    }

    for(const QJsonValue& val : obj["Values"].toArray())
        d.values.append(JsonToValue(val, t));

    return d;
}
