#include "PlaceholderNetworkPolicy.hpp"

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>


template<>
void Visitor<Reader<DataStream>>::readFrom(const Client& elt)
{
    readFrom(static_cast<const IdentifiedObject<Client>&>(elt));
    m_stream << elt.name();
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Client& elt)
{
    QString s;
    m_stream  >> s;
    elt.setName(s);

    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const Client& elt)
{
    readFrom(static_cast<const IdentifiedObject<Client>&>(elt));
    m_obj["Name"] = elt.name();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Client& elt)
{
    elt.setName(m_obj["Name"].toString());
}


template<>
void Visitor<Reader<DataStream>>::readFrom(const NetworkPluginPolicy& elt)
{
    m_stream << elt.session()->id();
    readFrom(static_cast<Client&>(elt.session()->localClient()));

    m_stream << elt.session()->remoteClients().count();
    for(auto& clt : elt.session()->remoteClients())
    {
        readFrom(static_cast<Client&>(*clt));
    }

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(PlaceholderNetworkPolicy& elt)
{
    id_type<Session> sessId;
    m_stream >> sessId;

    elt.m_session = new Session{new LocalClient(*this, nullptr), sessId, &elt};

    int n;
    m_stream >> n;
    for(; n --> 0;)
    {
        elt.m_session->addClient(new RemoteClient(*this, elt.m_session));
    }

    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const NetworkPluginPolicy& elt)
{
    m_obj["SessionId"] = toJsonValue(elt.session()->id());
    m_obj["LocalClient"] = toJsonObject(static_cast<Client&>(elt.session()->localClient()));

    QJsonArray arr;
    for(auto& clt : elt.session()->remoteClients())
    {
        arr.push_back(toJsonObject(static_cast<Client&>(*clt)));
    }
    m_obj["RemoteClients"] = arr;
}


template<>
void Visitor<Writer<JSONObject>>::writeTo(PlaceholderNetworkPolicy& elt)
{
    Deserializer<JSONObject> localClientDeser(m_obj["LocalClient"].toObject());
    elt.m_session = new Session{
            new LocalClient(localClientDeser, nullptr),
            fromJsonValue<id_type<Session>>(m_obj["SessionId"]),
            &elt};

    for(const auto& json_vref : m_obj["RemoteClients"].toArray())
    {
        Deserializer<JSONObject> deser(json_vref.toObject());
        elt.m_session->addClient(new RemoteClient(deser, elt.m_session));
    }

}
