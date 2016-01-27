#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "OSCSpecificSettings.hpp"

template <typename T> class Reader;
template <typename T> class Writer;


template<>
void Visitor<Reader<DataStream>>::readFrom_impl(const Ossia::OSCSpecificSettings& n)
{
    // TODO put it in the right order before 1.0 final.
    // TODO same for minuit, etc..
    m_stream << n.outputPort
             << n.inputPort
             << n.host;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Ossia::OSCSpecificSettings& n)
{
    m_stream >> n.outputPort
             >> n.inputPort
             >> n.host;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const Ossia::OSCSpecificSettings& n)
{
    m_obj["OutputPort"] = n.outputPort;
    m_obj["InputPort"] = n.inputPort;
    m_obj["Host"] = n.host;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Ossia::OSCSpecificSettings& n)
{
    n.outputPort = m_obj["OutputPort"].toInt();
    n.inputPort = m_obj["InputPort"].toInt();
    n.host = m_obj["Host"].toString();
}
