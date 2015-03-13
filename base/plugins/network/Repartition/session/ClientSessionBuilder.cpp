#include "ClientSessionBuilder.hpp"

#include "ClientSession.hpp"
#include "../client/RemoteClient.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>

ClientSessionBuilder::ClientSessionBuilder(QString ip, int port)
{
    m_mastersocket = new NetworkSocket(ip, port, nullptr);
    connect(m_mastersocket, &NetworkSocket::messageReceived,
            this, &ClientSessionBuilder::on_messageReceived);
}

void ClientSessionBuilder::initiateConnection()
{
    // Todo only call this if the socket is ready.
    NetworkMessage askId;
    askId.address = "/session/askNewId";
    m_mastersocket->sendMessage(askId);
}

ClientSession*ClientSessionBuilder::builtSession() const
{
    return m_session;
}

QByteArray ClientSessionBuilder::documentData() const
{
    return m_documentData;
}

void ClientSessionBuilder::on_messageReceived(NetworkMessage m)
{
    if(m.address == "/session/idOffer")
    {
        m_sessionId.setVal(m.sessionId); // The session offered
        m_masterId.setVal(m.clientId); // Message is from the master
        QDataStream s(m.data);
        s >> m_clientId; // The offered client id

        // TODO ask with additional data
        NetworkMessage join;
        join.address = "/session/join";
        join.clientId = m_clientId;
        join.sessionId = m.sessionId;

        m_mastersocket->sendMessage(join);
    }
    else if(m.address == "/session/document")
    {
        m_session = new ClientSession(new RemoteClient(m_mastersocket, m_masterId),
                                      new LocalClient(m_clientId),
                                      m_sessionId,
                                      nullptr);

        m_documentData = m.data;

        emit sessionReady(this, m_session);
    }
}
