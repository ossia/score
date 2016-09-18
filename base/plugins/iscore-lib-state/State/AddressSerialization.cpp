#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QDataStream>
#include <QtGlobal>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

#include "Address.hpp"

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const State::AccessorVector& a)
{
    int32_t n = a.size();
    m_stream << n;
    for(int32_t i = 0; i < n; i++)
        m_stream << (int32_t)a[i];

    insertDelimiter();
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Writer<DataStream>>::writeTo(State::AccessorVector& a)
{
    int32_t size;
    m_stream >> size;
    for(int i = 0; i < size; i++)
    {
        int32_t t;
        m_stream >> t;
        a.push_back(t);
    }

    checkDelimiter();
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const State::Address& a)
{
    m_stream << a.device << a.path;
    insertDelimiter();
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Reader<JSONObject>>::readFrom(const State::Address& a)
{
    m_obj[strings.Device] = a.device;
    m_obj[strings.Path] = a.path.join('/');
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Writer<DataStream>>::writeTo(State::Address& a)
{
    m_stream >> a.device >> a.path;
    checkDelimiter();
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Writer<JSONObject>>::writeTo(State::Address& a)
{
    a.device = m_obj[strings.Device].toString();

    auto path = m_obj[strings.Path].toString();

    if(!path.isEmpty())
        a.path = m_obj[strings.Path].toString().split('/');
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const State::AddressAccessor& rel)
{
    m_stream << rel.address << rel.accessors;

    insertDelimiter();
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Reader<JSONObject>>::readFrom(const State::AddressAccessor& rel)
{
    m_obj[strings.address] = toJsonObject(rel.address);
    m_obj["Accessors"] = toJsonValueArray(rel.accessors);
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Writer<DataStream>>::writeTo(State::AddressAccessor& rel)
{
    m_stream >> rel.address >> rel.accessors;

    checkDelimiter();
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Writer<JSONObject>>::writeTo(State::AddressAccessor& rel)
{
    fromJsonObject(m_obj[strings.address], rel.address);
    auto arr = m_obj["Accessors"].toArray();
    for(auto v : arr)
    {
        rel.accessors.push_back(v.toInt());
    }
}

