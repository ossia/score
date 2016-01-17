#pragma once
#include <boost/optional/optional.hpp>
#include <QObject>
#include <QString>

#include <iscore/tools/SettableIdentifier.hpp>

class QTcpSocket;

namespace Network
{
class RemoteClient;
struct NetworkMessage;
class Client;
class MasterSession;
class NetworkSocket;

class RemoteClientBuilder : public QObject
{
        Q_OBJECT
    public:
        RemoteClientBuilder(MasterSession& session, QTcpSocket* sock);

    signals:
        void clientReady(RemoteClientBuilder* builder, RemoteClient*);

    public slots:
        void on_messageReceived(const NetworkMessage& m);


    private:
        MasterSession& m_session;
        NetworkSocket* m_socket;
        RemoteClient* m_remoteClient{};

        Id<Client> m_clientId;
        QString m_clientName;
};
}
