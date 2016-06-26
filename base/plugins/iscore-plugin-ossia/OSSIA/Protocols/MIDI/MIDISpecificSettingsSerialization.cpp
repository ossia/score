#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "MIDISpecificSettings.hpp"
#include <iscore/serialization/JSONValueVisitor.hpp>

template <typename T> class Reader;
template <typename T> class Writer;


template<>
void Visitor<Reader<DataStream>>::readFrom_impl(const Ossia::MIDISpecificSettings& n)
{
    m_stream << n.io << n.endpoint << n.port;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Ossia::MIDISpecificSettings& n)
{
    m_stream >> n.io >> n.endpoint >> n.port;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const Ossia::MIDISpecificSettings& n)
{
    m_obj["IO"] = toJsonValue(n.io);
    m_obj["Endpoint"] = n.endpoint;
    m_obj["Port"] = (int) n.port;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Ossia::MIDISpecificSettings& n)
{
    fromJsonValue(m_obj["IO"], n.io);
    n.endpoint = m_obj["Endpoint"].toString();
    n.port = m_obj["Port"].toInt();
}
