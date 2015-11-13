#include "RemoteClientBuilder.hpp"
#include <Serialization/NetworkSocket.hpp>
#include "MasterSession.hpp"
#include <core/document/Document.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
RemoteClientBuilder::RemoteClientBuilder(MasterSession& session, QTcpSocket* sock):
    m_session{session}
{
    m_socket = new NetworkSocket(sock, nullptr);
    connect(m_socket, SIGNAL(messageReceived(NetworkMessage)),
            this, SLOT(on_messageReceived(NetworkMessage)));
}

void RemoteClientBuilder::on_messageReceived(const NetworkMessage& m)
{
    if(m.address == "/session/askNewId")
    {
        QDataStream s{m.data};
        s >> m_clientName;

        // TODO validation
        NetworkMessage idOffer;
        idOffer.address = "/session/idOffer";
        idOffer.sessionId = m_session.id().val().get();
        idOffer.clientId = m_session.localClient().id().val().get();
        {
            QDataStream s(&idOffer.data, QIODevice::WriteOnly);

            // TODO make a strong id with the client array!!!!!!
            int32_t id = iscore::random_id_generator::getRandomId();
            m_clientId = Id<Client>(id);
            s << id;
        }

        m_socket->sendMessage(idOffer);

    }
    else if(m.address == "/session/join")
    {
        // TODO validation
        NetworkMessage doc;
        doc.address = "/session/document";

        // Data is the serialized command stack, and the document models.
        auto& cq = m_session.document()->commandStack();
        QList<QPair <QPair <CommandParentFactoryKey, CommandFactoryKey>, QByteArray> > commandStack; // TODO use a strong SerializedCommandStack for this
        for(int i = 0; i < cq.size(); i++)
        {
            auto cmd = cq.command(i);
            commandStack.push_back({{cmd->parentKey(), cmd->key()}, cmd->serialize()});
        }

        // TODO also transmit the position in the command stack (else if somebody undoes and transmits it will crash).
        // Or use the CommandStackSave file.
        QDataStream s{&doc.data, QIODevice::WriteOnly};
        s << commandStack << m_session.document()->saveAsByteArray();

        m_socket->sendMessage(doc);

        m_remoteClient = new RemoteClient(m_socket, m_clientId);
        m_remoteClient->setName(m_clientName);
        emit clientReady(this, m_remoteClient);
    }
}
