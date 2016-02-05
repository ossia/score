
#include <iscore/serialization/VisitorCommon.hpp>
#include <QDataStream>
#include <QtGlobal>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector>
#include <algorithm>
#include <array>
#include <cstddef>

#include "MessageNode.hpp"
#include <State/Value.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

namespace Process { class ProcessModel; }
namespace boost {
template <class T> class optional;
}  // namespace boost
template <typename T> class Reader;
template <typename T> class Writer;

template<typename T>
void toJsonValue(
        QJsonObject& object,
        const QString& name,
        const boost::optional<T>& value)
{
    if(value)
    {
        object[name] = marshall<JSONObject>(*value);
    }
}

template<typename T, std::size_t N>
QJsonArray toJsonArray(const std::array<T, N>& array)
{
    QJsonArray arr;
    for(std::size_t i = 0; i < N; i++)
    {
        arr.append(toJsonValue(array[i]));
    }

    return arr;
}

template<typename T, std::size_t N>
void fromJsonArray(const QJsonArray& array, std::array<T, N>& res)
{
    for(std::size_t i = 0; i < N; i++)
    {
        res[i] = fromJsonValue<T>(array[i]);
    }
}


template<typename T>
void fromJsonValue(
        const QJsonObject& object,
        const QString& name,
        boost::optional<T>& value)
{
    auto it = object.find(name);
    if(it != object.end())
    {
        value = unmarshall<State::Value>((*it).toObject());
    }
    else
    {
        value.reset();
    }
}




template<>
void Visitor<Reader<DataStream>>::readFrom(
        const std::array<Process::PriorityPolicy, 3>& val)
{
    for(int i = 0; i < 3; i++)
        m_stream << val[i];
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        std::array<Process::PriorityPolicy, 3>& val)
{
    for(int i = 0; i < 3; i++)
        m_stream >> val[i];
}

template<>
void Visitor<Reader<DataStream>>::readFrom(
        const Process::ProcessStateData& val)
{
    m_stream << val.process << val.value;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Process::ProcessStateData& val)
{
    m_stream >> val.process >> val.value;
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(
        const Process::ProcessStateData& val)
{
    m_obj["Process"] = toJsonValue(val.process);
    toJsonValue(m_obj, "Value", val.value);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Process::ProcessStateData& val)
{
    val.process = fromJsonValue<Id<Process::ProcessModel>>(m_obj["Process"]);
    fromJsonValue(m_obj, "Value", val.value);
}


template<>
void Visitor<Reader<DataStream>>::readFrom(
        const Process::StateNodeValues& val)
{
    m_stream << val.previousProcessValues << val.followingProcessValues << val.userValue << val.priorities;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Process::StateNodeValues& val)
{
    m_stream >> val.previousProcessValues >> val.followingProcessValues >> val.userValue >> val.priorities;
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(
        const Process::StateNodeValues& val)
{
    m_obj["Previous"] = toJsonArray(val.previousProcessValues);
    m_obj["Following"] = toJsonArray(val.followingProcessValues);
    toJsonValue(m_obj, "User", val.userValue);
    m_obj["Priorities"] = toJsonArray(val.priorities);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Process::StateNodeValues& val)
{
    fromJsonArray(m_obj["Previous"].toArray(), val.previousProcessValues);
    fromJsonArray(m_obj["Following"].toArray(), val.followingProcessValues);
    fromJsonValue(m_obj, "User", val.userValue);
    fromJsonArray(m_obj["Priorities"].toArray(), val.priorities);
}


template<>
ISCORE_LIB_PROCESS_EXPORT void Visitor<Reader<DataStream>>::readFrom(
        const Process::StateNodeData& node)
{
    m_stream << node.name << node.values;
    insertDelimiter();
}

template<>
ISCORE_LIB_PROCESS_EXPORT void Visitor<Writer<DataStream>>::writeTo(
        Process::StateNodeData& node)
{
    m_stream >> node.name >> node.values;
    checkDelimiter();
}

template<>
ISCORE_LIB_PROCESS_EXPORT void Visitor<Reader<JSONObject>>::readFrom(
        const Process::StateNodeData& node)
{
    m_obj["Name"] = node.name;
    readFrom(node.values);
}

template<>
ISCORE_LIB_PROCESS_EXPORT void Visitor<Writer<JSONObject>>::writeTo(
        Process::StateNodeData& node)
{
    node.name = m_obj["Name"].toString();
    writeTo(node.values);
}
