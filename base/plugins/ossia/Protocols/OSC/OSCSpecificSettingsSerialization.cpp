#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "OSCSpecificSettings.hpp"


template<>
void Visitor<Reader<DataStream>>::readFrom(const OSCSpecificSettings& n)
{
    m_stream << n.outputPort
             << n.inputPort
             << n.host;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(OSCSpecificSettings& n)
{
    m_stream >> n.outputPort
             >> n.inputPort
             >> n.host;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const OSCSpecificSettings& n)
{
    m_obj["OutputPort"] = n.outputPort;
    m_obj["InputPort"] = n.inputPort;
    m_obj["Host"] = n.host;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(OSCSpecificSettings& n)
{
    n.outputPort = m_obj["OutputPort"].toInt();
    n.inputPort = m_obj["InputPort"].toInt();
    n.host = m_obj["Host"].toString();
}
