
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QDataStream>
#include <QtGlobal>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <algorithm>

#include "Group.hpp"
#include <iscore/serialization/JSONValueVisitor.hpp>

template <typename T> class IdentifiedObject;
template <typename T> class Reader;
template <typename T> class Writer;


template<>
void Visitor<Reader<DataStream>>::readFrom(
        const Network::Group& elt)
{
    readFrom(static_cast<const IdentifiedObject<Network::Group>&>(elt));
    m_stream << elt.name() << elt.clients();
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Network::Group& elt)
{
    m_stream >> elt.m_name >> elt.m_executingClients;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(
        const Network::Group& elt)
{
    readFrom(static_cast<const IdentifiedObject<Network::Group>&>(elt));
    m_obj["Name"] = elt.name();
    m_obj["Clients"] = toJsonArray(elt.clients());
}


template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Network::Group& elt)
{
    elt.m_name = m_obj["Name"].toString();
    fromJsonValueArray(m_obj["Clients"].toArray(), elt.m_executingClients);
}
