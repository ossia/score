#include <boost/core/explicit_operator_bool.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>
#include <qlist.h>
#include <algorithm>

#include "DocumentPlugins/NetworkDocumentPlugin.hpp"
#include "PlaceholderNetworkPolicy.hpp"
#include "iscore/serialization/JSONValueVisitor.hpp"
#include "iscore/tools/SettableIdentifier.hpp"
#include "session/../client/LocalClient.hpp"
#include "session/../client/RemoteClient.hpp"
#include "session/Session.hpp"

class Client;
template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore_plugin_networkPolicy& elt)
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
void Visitor<Reader<JSONObject>>::readFrom(const iscore_plugin_networkPolicy& elt)
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
void Visitor<Writer<DataStream>>::writeTo(PlaceholderNetworkPolicy& elt)
{
    Id<Session> sessId;
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
void Visitor<Writer<JSONObject>>::writeTo(PlaceholderNetworkPolicy& elt)
{
    Deserializer<JSONObject> localClientDeser(m_obj["LocalClient"].toObject());
    elt.m_session = new Session{
            new LocalClient(localClientDeser, nullptr),
            fromJsonValue<Id<Session>>(m_obj["SessionId"]),
            &elt};

    for(const auto& json_vref : m_obj["RemoteClients"].toArray())
    {
        Deserializer<JSONObject> deser(json_vref.toObject());
        elt.m_session->addClient(new RemoteClient(deser, elt.m_session));
    }

}
