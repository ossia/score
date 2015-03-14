#include "Session.hpp"

#include <Serialization/MessageMapper.hpp>
#include <Serialization/MessageValidator.hpp>

Session::Session(LocalClient* client, id_type<Session> id, QObject* parent):
    IdentifiedObject<Session>{id, "Session", parent},
    m_client{client},
    m_mapper{new MessageMapper},
    m_validator{new MessageValidator(*this, mapper())}
{
}


NetworkMessage Session::makeMessage(QString address)
{
    NetworkMessage m;
    m.address = address;
    m.clientId = localClient().id();
    m.sessionId = id();

    return m;
}


void Session::validateMessage(NetworkMessage m)
{
    if(validator().validate(m))
        mapper().map(m);
}
