#include "NetworkMessage.hpp"
#include "../Repartition/session/Session.hpp"

QDataStream& operator<<(QDataStream& s, const NetworkMessage& m)
{
    s << m.address << m.sessionId << m.clientId << m.data;
    return s;
}

QDataStream& operator>>(QDataStream& s, NetworkMessage& m)
{
    s >> m.address >> m.sessionId >> m.clientId >> m.data;
    return s;
}

NetworkMessage::NetworkMessage(Session& s, QString&& addr, QByteArray&& data):
    address{addr},
    sessionId{s.id()},
    clientId{s.localClient().id().val().get()},
    data{data}
{
}
