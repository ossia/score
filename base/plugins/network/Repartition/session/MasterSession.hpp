#pragma once
#include "Session.hpp"

#include "RemoteClientBuilder.hpp"
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
                      id_type<Session> id,
                      QObject* parent = nullptr);

        void broadcast(NetworkMessage m);
        void transmit(id_type<Client> sender, NetworkMessage m);

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
