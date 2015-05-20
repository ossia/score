#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "MinuitSpecificSettings.hpp"


template<>
void Visitor<Reader<DataStream>>::readFrom(const MinuitSpecificSettings& n)
{
    m_stream << n.host <<  n.inPort << n.outPort;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(MinuitSpecificSettings& n)
{
    m_stream >> n.host >> n.inPort >> n.outPort;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const MinuitSpecificSettings& n)
{
    m_obj["InPort"] = n.inPort;
    m_obj["OutPort"] = n.outPort;
    m_obj["Host"] = n.host;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(MinuitSpecificSettings& n)
{
    n.inPort = m_obj["InPort"].toInt();
    n.outPort = m_obj["OutPort"].toInt();
    n.host = m_obj["Host"].toString();
}
