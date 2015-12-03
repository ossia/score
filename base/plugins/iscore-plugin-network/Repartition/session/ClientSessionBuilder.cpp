#include <iscore/serialization/DataStreamVisitor.hpp>
#include <QDataStream>
#include <QIODevice>
#include <sys/types.h>

#include "ClientSession.hpp"
#include "ClientSessionBuilder.hpp"
#include "Serialization/NetworkMessage.hpp"
#include "Serialization/NetworkSocket.hpp"
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/document/Document.hpp>
#include <core/command/CommandStackSerialization.hpp>
#include "session/../client/LocalClient.hpp"
#include "session/../client/RemoteClient.hpp"
#include "DocumentPlugins/NetworkDocumentPlugin.hpp"
#include "DocumentPlugins/NetworkClientDocumentPlugin.hpp"

ClientSessionBuilder::ClientSessionBuilder(
        const iscore::ApplicationContext& ctx,
        QString ip,
        int port):
    m_context{ctx}
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

const std::vector<iscore::CommandData>& ClientSessionBuilder::commandStackData() const
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

        // We start building our document.
        Visitor<Writer<DataStream>> writer{m.data};
        writer.m_stream >> m_documentData;

        // The SessionBuilder should have a saved document and saved command list.
        // However there is a difference with what happens when there is a crash :
        // Here the document is sent as it is in its current state. The CommandList only serves
        // in case somebody does undo, so that the computer who joined later can still
        // undo, too.

        iscore::Document* doc = m_context.documents.loadDocument(
                       m_context,
                       m_documentData,
                       m_context.components.availableDocuments().front()); // TODO id instead

        if(!doc)
        {
            qDebug() << "Invalid document received";
            delete m_session;
            m_session = nullptr;

            emit sessionFailed();
            return;
        }

        loadCommandStack(
                    m_context.components,
                    writer,
                    doc->commandStack(),
                    [] (auto) { }); // No redo.

        auto& np = doc->context().plugin<NetworkDocumentPlugin>();
        np.setPolicy(new ClientNetworkPolicy{m_session, doc});

        emit sessionReady();
    }
}
