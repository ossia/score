#include "RemoteClientBuilder.hpp"
#include <Serialization/NetworkSocket.hpp>
#include "MasterSession.hpp"
#include <core/document/Document.hpp>
RemoteClientBuilder::RemoteClientBuilder(MasterSession& session, QTcpSocket* sock):
    m_session{session}
{
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
        idOffer.sessionId = m_session.id().val().get();
        idOffer.clientId = m_session.localClient().id().val().get();
        {
            QDataStream s(&idOffer.data, QIODevice::WriteOnly);

            int32_t id = getNextId();
            m_clientId = id_type<Client>(id);
            s << id;
        }

        m_socket->sendMessage(idOffer);
    }
    else if(m.address == "/session/join")
    {
        NetworkMessage doc;
        doc.address = "/session/document";

        // Data is the serialized command stack, and the document models.
        auto& cq = m_session.document()->commandStack();
        QList<QPair <QPair <QString,QString>, QByteArray> > commandStack;
        for(int i = 0; i < cq.size(); i++)
        {
            auto cmd = cq.command(i);
            commandStack.push_back({{cmd->parentName(), cmd->name()}, cmd->serialize()});
        }

        QDataStream s{&doc.data, QIODevice::WriteOnly};
        s << commandStack << m_session.document()->saveAsByteArray();

        m_socket->sendMessage(doc);

        m_remoteClient = new RemoteClient(m_socket, m_clientId);
        emit clientReady(this, m_remoteClient);
    }
}
