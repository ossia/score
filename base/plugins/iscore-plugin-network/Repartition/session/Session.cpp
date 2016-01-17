#include <Serialization/MessageMapper.hpp>
#include <Serialization/MessageValidator.hpp>
#include <boost/optional/optional.hpp>

#include "Serialization/NetworkMessage.hpp"
#include "Session.hpp"
#include "session/../client/LocalClient.hpp"

class QObject;

namespace Network
{
Session::Session(LocalClient* client, Id<Session> id, QObject* parent):
    IdentifiedObject<Session>{id, "Session", parent},
    m_client{client},
    m_mapper{new MessageMapper},
    m_validator{new MessageValidator(*this, mapper())}
{
    m_client->setParent(this);
}

Session::~Session()
{
    delete m_mapper;
    delete m_validator;
}

NetworkMessage Session::makeMessage(QString address)
{
    NetworkMessage m;
    m.address = address;
    m.clientId = localClient().id().val().get();
    m.sessionId = id().val().get();

    return m;
}


void Session::validateMessage(NetworkMessage m)
{
    if(validator().validate(m))
        mapper().map(m);
}
}
