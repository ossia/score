#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "MinuitSpecificSettings.hpp"

template <typename T> class Reader;
template <typename T> class Writer;


template<>
void Visitor<Reader<DataStream>>::readFrom_impl(const Ossia::MinuitSpecificSettings& n)
{
    m_stream << n.host <<  n.inputPort << n.outputPort;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Ossia::MinuitSpecificSettings& n)
{
    m_stream >> n.host >> n.inputPort >> n.outputPort;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const Ossia::MinuitSpecificSettings& n)
{
    m_obj["InPort"] = n.inputPort;
    m_obj["OutPort"] = n.outputPort;
    m_obj["Host"] = n.host;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Ossia::MinuitSpecificSettings& n)
{
    n.inputPort = m_obj["InPort"].toInt();
    n.outputPort = m_obj["OutPort"].toInt();
    n.host = m_obj["Host"].toString();
}
