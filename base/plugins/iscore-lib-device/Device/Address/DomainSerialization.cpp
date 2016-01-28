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
    obj["Min"] = ValueToJson(d.min);
    obj["Max"] = ValueToJson(d.max);

    QJsonArray arr;
    for(auto& val : d.values)
        arr.append(ValueToJson(val));
    obj["Values"] = arr;

    return obj;
}

Device::Domain JsonToDomain(const QJsonObject& obj, const QString& t)
{
    Device::Domain d;
    if(obj.contains("Min"))
    {
        d.min = State::convert::toValue(obj["Min"], t);
    }
    if(obj.contains("Max"))
    {
        d.max = State::convert::toValue(obj["Max"], t);
    }

    for(const QJsonValue& val : obj["Values"].toArray())
        d.values.append(State::convert::toValue(val, t));

    return d;
}
}
