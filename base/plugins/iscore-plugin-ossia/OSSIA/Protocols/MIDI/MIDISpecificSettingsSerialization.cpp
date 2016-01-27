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
    m_stream << n.io << n.endpoint;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Ossia::MIDISpecificSettings& n)
{
    m_stream >> n.io >> n.endpoint;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const Ossia::MIDISpecificSettings& n)
{
    m_obj["IO"] = toJsonValue(n.io);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Ossia::MIDISpecificSettings& n)
{
    fromJsonValue(m_obj["IO"], n.io);
}
