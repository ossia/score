
#include <boost/optional/optional.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <algorithm>

#include "DocumentPlugins/NetworkDocumentPlugin.hpp"
#include "PlaceholderNetworkPolicy.hpp"
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include "session/../client/LocalClient.hpp"
#include "session/../client/RemoteClient.hpp"
#include "session/Session.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

// Move me
template<>
void Visitor<Reader<DataStream>>::readFrom(
        const Network::NetworkPolicyInterface& elt)
{
    m_stream << elt.session()->id();
    readFrom(static_cast<Network::Client&>(elt.session()->localClient()));

    m_stream << elt.session()->remoteClients().count();
    for(auto& clt : elt.session()->remoteClients())
    {
        readFrom(static_cast<Network::Client&>(*clt));
    }

    insertDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(
        const Network::NetworkPolicyInterface& elt)
{
    m_obj["SessionId"] = toJsonValue(elt.session()->id());
    m_obj["LocalClient"] = toJsonObject(static_cast<Network::Client&>(elt.session()->localClient()));

    QJsonArray arr;
    for(auto& clt : elt.session()->remoteClients())
    {
        arr.push_back(toJsonObject(static_cast<Network::Client&>(*clt)));
    }
    m_obj["RemoteClients"] = arr;
}



template<>
void Visitor<Writer<DataStream>>::writeTo(
        Network::PlaceholderNetworkPolicy& elt)
{
    Id<Network::Session> sessId;
    m_stream >> sessId;

    elt.m_session = new Network::Session{new Network::LocalClient(*this, nullptr), sessId, &elt};

    int n;
    m_stream >> n;
    for(; n --> 0;)
    {
        elt.m_session->addClient(new Network::RemoteClient(*this, elt.m_session));
    }

    checkDelimiter();
}



template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Network::PlaceholderNetworkPolicy& elt)
{
    Deserializer<JSONObject> localClientDeser(m_obj["LocalClient"].toObject());
    elt.m_session = new Network::Session{
            new Network::LocalClient(localClientDeser, nullptr),
            fromJsonValue<Id<Network::Session>>(m_obj["SessionId"]),
            &elt};

    for(const auto& json_vref : m_obj["RemoteClients"].toArray())
    {
        Deserializer<JSONObject> deser(json_vref.toObject());
        elt.m_session->addClient(new Network::RemoteClient(deser, elt.m_session));
    }

}
