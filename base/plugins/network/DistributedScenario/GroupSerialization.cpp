#include "Group.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>


template<>
void Visitor<Reader<DataStream>>::readFrom(const Group& elt)
{
    readFrom(static_cast<const IdentifiedObject<Group>&>(elt));
    m_stream << elt.name() << elt.clients();
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Group& elt)
{
    m_stream >> elt.m_name >> elt.m_executingClients;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const Group& elt)
{
    readFrom(static_cast<const IdentifiedObject<Group>&>(elt));
    m_obj["Name"] = elt.name();
    m_obj["Clients"] = toJsonArray(elt.clients());
}


template<>
void Visitor<Writer<JSONObject>>::writeTo(Group& elt)
{
    elt.m_name = m_obj["Name"].toString();
    fromJsonValueArray(m_obj["Clients"].toArray(), elt.m_executingClients);
}
