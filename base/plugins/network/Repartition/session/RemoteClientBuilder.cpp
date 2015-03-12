#include "RemoteClientBuilder.hpp"
#include <Serialization/NetworkSocket.hpp>
#include "MasterSession.hpp"
#include <core/document/Document.hpp>
RemoteClientBuilder::RemoteClientBuilder(MasterSession& session, QTcpSocket* sock):
    m_session{session}
{
    qDebug() << "bump";
    m_socket = new NetworkSocket(sock, nullptr);
    connect(m_socket, SIGNAL(messageReceived(NetworkMessage)),
            this, SLOT(on_messageReceived(NetworkMessage)));
}

void RemoteClientBuilder::on_messageReceived(NetworkMessage m)
{
    if(m.address == "/session/askNewId")
    {
        NetworkMessage idOffer;
        idOffer.address = "/session/idOffer";
        idOffer.sessionId = m_session.id();
        idOffer.clientId = m_session.localClient().id();
        QDataStream s(&idOffer.data, QIODevice::WriteOnly);

        m_clientId = id_type<Client>(getNextId());
        s << m_clientId.val().get();

        m_socket->sendMessage(idOffer);
    }
    else if(m.address == "/session/join")
    {
        NetworkMessage doc;
        doc.address = "/session/document";
        doc.data = m_session.document()->saveDocumentModelAsByteArray();
        m_socket->sendMessage(doc);

        m_remoteClient = new RemoteClient(m_socket, m_clientId);
        emit clientReady(this, m_remoteClient);
    }
}
