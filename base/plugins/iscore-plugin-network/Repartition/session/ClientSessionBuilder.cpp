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
    {
        QDataStream s{&askId.data, QIODevice::WriteOnly};
        s << m_clientName;
    }

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

const QList<QPair<QPair<std::string, std::string>, QByteArray> >& ClientSessionBuilder::commandStackData() const
{
    return m_commandStack;
}

void ClientSessionBuilder::on_messageReceived(const NetworkMessage& m)
{
    if(m.address == "/session/idOffer")
    {
        m_sessionId.setVal(m.sessionId); // The session offered
        m_masterId.setVal(m.clientId); // Message is from the master
        QDataStream s(m.data);
        int32_t id;
        s >> id; // The offered client id
        m_clientId = Id<Client>(id);

        NetworkMessage join;
        join.address = "/session/join";
        join.clientId = m_clientId.val().get();
        join.sessionId = m.sessionId;

        m_mastersocket->sendMessage(join);
    }
    else if(m.address == "/session/document")
    {
        auto remoteClient = new RemoteClient(m_mastersocket, m_masterId);
        remoteClient->setName("RemoteMaster");
        m_session = new ClientSession(remoteClient,
                                      new LocalClient(m_clientId),
                                      m_sessionId,
                                      nullptr);
        m_session->localClient().setName(m_clientName);

        QDataStream s{m.data};
        s >> m_commandStack >> m_documentData;

        emit sessionReady(this, m_session);
    }
}
