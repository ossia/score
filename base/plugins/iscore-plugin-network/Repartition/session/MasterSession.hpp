#pragma once
#include <QList>

#include "Session.hpp"

class Client;
class LocalClient;
class QObject;
class QTcpSocket;
class RemoteClient;
class RemoteClientBuilder;
struct NetworkMessage;
#include <iscore/tools/SettableIdentifier.hpp>

namespace iscore {
class Document;
}

#ifdef USE_ZEROCONF
namespace KDNSSD
{
    class PublicService;
}
#endif

class MasterSession : public Session
{
           Q_OBJECT
    public:
        MasterSession(iscore::Document* doc,
                      LocalClient* theclient,
                      Id<Session> id,
                      QObject* parent = nullptr);

        void broadcast(NetworkMessage m);
        void transmit(Id<Client> sender, NetworkMessage m);

        iscore::Document* document() const
        { return m_document; }

    public slots:
        void on_createNewClient(QTcpSocket* sock);
        void on_clientReady(RemoteClientBuilder* bldr, RemoteClient* clt);

    private:
        iscore::Document* m_document{};
        QList<RemoteClientBuilder*> m_clientBuilders;

#ifdef USE_ZEROCONF
        KDNSSD::PublicService* m_service{};
#endif

};
