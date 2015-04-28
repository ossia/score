#include "NetworkDocumentPlugin.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>


template<>
void Visitor<Reader<DataStream>>::readFrom(const NetworkDocumentPlugin& elt)
{
    readFrom(static_cast<const NamedObject&>(elt));
    readFrom(*elt.groupManager());

    // Note : we do not save the policy since it will be different on each computer.
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(NetworkDocumentPlugin& elt)
{
    //m_stream >> elt.m_name >> elt.m_executingClients;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const NetworkDocumentPlugin& elt)
{
    readFrom(static_cast<const NamedObject&>(elt));
    //m_obj["Clients"] = toJsonArray(elt.clients());
}


template<>
void Visitor<Writer<JSONObject>>::writeTo(NetworkDocumentPlugin& elt)
{
    //fromJsonValueArray(m_obj["Clients"].toArray(), elt.m_executingClients);
}
