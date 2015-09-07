#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "MIDISpecificSettings.hpp"


template<>
void Visitor<Reader<DataStream>>::readFrom(const MIDISpecificSettings& n)
{
    m_stream << n.io << n.endpoint;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(MIDISpecificSettings& n)
{
    m_stream >> n.io >> n.endpoint;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const MIDISpecificSettings& n)
{
    m_obj["IO"] = toJsonValue(n.io);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(MIDISpecificSettings& n)
{
    fromJsonValue(m_obj["IO"], n.io);
}
