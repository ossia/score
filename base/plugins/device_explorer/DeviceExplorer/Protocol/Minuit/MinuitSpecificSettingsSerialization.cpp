#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "MinuitSpecificSettings.hpp"


template<>
void Visitor<Reader<DataStream>>::readFrom(const MinuitSpecificSettings& n)
{
    m_stream << n.port << n.host;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(MinuitSpecificSettings& n)
{
    m_stream >> n.port >> n.host;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSON>>::readFrom(const MinuitSpecificSettings& n)
{
    m_obj["Port"] = n.port;
    m_obj["Host"] = n.host;
}

template<>
void Visitor<Writer<JSON>>::writeTo(MinuitSpecificSettings& n)
{
    n.port = m_obj["Port"].toInt();
    n.host = m_obj["Host"].toString();
}
