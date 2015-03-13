#pragma once
#include "Session.hpp"
#include "ZeroConfServerThread.h"
#include "RemoteClientBuilder.hpp"
namespace iscore {
class Document;
}
class MasterSession : public Session
{
        ZeroConfServerThread _zc_thread;

           Q_OBJECT
    public:
        MasterSession(iscore::Document* doc,
                      LocalClient* theclient,
                      id_type<Session> id,
                      QObject* parent = nullptr):
            Session{theclient, id, parent},
            m_document{doc}
        {
            connect(&localClient(), SIGNAL(createNewClient(QTcpSocket*)),
                    this, SLOT(on_createNewClient(QTcpSocket*)));
        }

        virtual ~MasterSession()
        {
            _zc_thread.quit();
            while(_zc_thread.isRunning()) std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }


        void broadcast(NetworkMessage m)
        {
            for(RemoteClient* client : remoteClients())
                client->sendMessage(m);
        }

        void transmit(id_type<RemoteClient> sender, NetworkMessage m)
        {
            for(auto& client : remoteClients())
            {
                if(client->id() != sender)
                    client->sendMessage(m);
            }
        }

        iscore::Document* document() const
        { return m_document; }

    public slots:
        void on_createNewClient(QTcpSocket* sock)
        {
            RemoteClientBuilder* builder = new RemoteClientBuilder(*this, sock);
            connect(builder, SIGNAL(clientReady(RemoteClientBuilder*,RemoteClient*)),
                    this, SLOT(on_clientReady(RemoteClientBuilder*,RemoteClient*)));

            m_clientBuilders.append(builder);
        }

        void on_clientReady(RemoteClientBuilder* bldr, RemoteClient* clt)
        {
            m_clientBuilders.removeOne(bldr);
            delete bldr;

            connect(clt, &RemoteClient::messageReceived,
                    this, &Session::validateMessage, Qt::QueuedConnection);

            remoteClients().push_back(clt);
        }

    private:
        iscore::Document* m_document{};
        QList<RemoteClientBuilder*> m_clientBuilders;

};
