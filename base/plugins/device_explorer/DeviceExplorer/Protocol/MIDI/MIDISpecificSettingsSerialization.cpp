#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "MIDISpecificSettings.hpp"


template<>
void Visitor<Reader<DataStream>>::readFrom(const MIDISpecificSettings& n)
{
    m_stream << static_cast<int>(n.io) << n.endpoint;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(MIDISpecificSettings& n)
{
    int io;
    m_stream >> io >> n.endpoint;
    n.io = static_cast<MIDISpecificSettings::IO>(io);
    checkDelimiter();
}

template<>
void Visitor<Reader<JSON>>::readFrom(const MIDISpecificSettings& n)
{
    m_obj["IO"] = static_cast<int>(n.io);
}

template<>
void Visitor<Writer<JSON>>::writeTo(MIDISpecificSettings& n)
{
    n.io = static_cast<MIDISpecificSettings::IO>(m_obj["IO"].toInt());
}
