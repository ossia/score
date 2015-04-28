#include "ClientSessionBuilder.hpp"

#include "ClientSession.hpp"
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

const QList<QPair<QPair<QString, QString>, QByteArray> >& ClientSessionBuilder::commandStackData() const
{
    return m_commandStack;
}

void ClientSessionBuilder::on_messageReceived(NetworkMessage m)
{
    if(m.address == "/session/idOffer")
    {
        m_sessionId.setVal(m.sessionId); // The session offered
        m_masterId.setVal(m.clientId); // Message is from the master
        QDataStream s(m.data);
        int32_t id;
        s >> id; // The offered client id
        m_clientId = id_type<Client>(id);

        // TODO ask with additional data
        NetworkMessage join;
        join.address = "/session/join";
        join.clientId = m_clientId.val().get();
        join.sessionId = m.sessionId;

        m_mastersocket->sendMessage(join);
    }
    else if(m.address == "/session/document")
    {
        auto remoteClient = new RemoteClient(m_mastersocket, m_masterId);
        // TODO transmit the name
        remoteClient->setName("RemoteTODO");
        m_session = new ClientSession(remoteClient,
                                      new LocalClient(m_clientId),
                                      m_sessionId,
                                      nullptr);


        QDataStream s{m.data};
        s >> m_commandStack >> m_documentData;

        emit sessionReady(this, m_session);
    }
}
