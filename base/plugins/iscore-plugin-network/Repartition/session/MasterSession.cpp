#include "MasterSession.hpp"
#ifdef USE_ZEROCONF
#include <KF5/KDNSSD/DNSSD/PublicService>
#endif

MasterSession::MasterSession(iscore::Document* doc, LocalClient* theclient, id_type<Session> id, QObject* parent):
    Session{theclient, id, parent},
    m_document{doc}
{
    con(localClient(), SIGNAL(createNewClient(QTcpSocket*)),
            this, SLOT(on_createNewClient(QTcpSocket*)));


#ifdef USE_ZEROCONF
    auto service = new KDNSSD::PublicService("Session i-score",
                                             "_iscore._tcp",
                                             localClient().localPort());
    service->publishAsync();
#endif
}

void MasterSession::broadcast(NetworkMessage m)
{
    for(RemoteClient* client : remoteClients())
        client->sendMessage(m);
}

void MasterSession::transmit(id_type<Client> sender, NetworkMessage m)
{
    for(const auto& client : remoteClients())
    {
        if(client->id() != sender)
            client->sendMessage(m);
    }
}

void MasterSession::on_createNewClient(QTcpSocket* sock)
{
    RemoteClientBuilder* builder = new RemoteClientBuilder(*this, sock);
    connect(builder, SIGNAL(clientReady(RemoteClientBuilder*,RemoteClient*)),
            this, SLOT(on_clientReady(RemoteClientBuilder*,RemoteClient*)));

    m_clientBuilders.append(builder);
}

void MasterSession::on_clientReady(RemoteClientBuilder* bldr, RemoteClient* clt)
{
    m_clientBuilders.removeOne(bldr);
    delete bldr;

    connect(clt, &RemoteClient::messageReceived,
            this, &Session::validateMessage, Qt::QueuedConnection);

    addClient(clt);
}
