#include <State/ValueConversion.hpp>
#include <State/ValueSerialization.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <QDataStream>
#include <QtGlobal>
#include <QJsonArray>
#include <QJsonValue>

#include "DomainSerialization.hpp"
#include <iscore_lib_device_export.h>
#include <State/Value.hpp>

template <typename T> class Reader;
template <typename T> class Writer;

template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const Device::Domain& n)
{
    m_stream << n.min << n.max << n.values;
}

template<>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Writer<DataStream>>::writeTo(Device::Domain& n)
{
    m_stream >> n.min >> n.max >> n.values;
}
namespace Device
{

QJsonObject DomainToJson(const Device::Domain& d)
{
    QJsonObject obj;
    obj[iscore::StringConstant().Min] = ValueToJson(d.min);
    obj[iscore::StringConstant().Max] = ValueToJson(d.max);

    QJsonArray arr;
    for(auto& val : d.values)
        arr.append(ValueToJson(val));
    obj[iscore::StringConstant().Values] = arr;

    return obj;
}

Device::Domain JsonToDomain(const QJsonObject& obj, const QString& t)
{
    Device::Domain d;

    auto min_it = obj.constFind(iscore::StringConstant().Min);
    if(min_it != obj.constEnd())
    {
        d.min = State::convert::fromQJsonValue(*min_it, t);
    }

    auto max_it = obj.constFind(iscore::StringConstant().Max);
    if(max_it != obj.constEnd())
    {
        d.max = State::convert::fromQJsonValue(*max_it, t);
    }

    for(const QJsonValue& val : obj[iscore::StringConstant().Values].toArray())
        d.values.append(State::convert::fromQJsonValue(val, t));

    return d;
}
}
