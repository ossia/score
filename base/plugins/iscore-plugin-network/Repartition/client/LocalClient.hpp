#pragma once
#include "Client.hpp"
#include <Serialization/NetworkServer.hpp>
// Has a TCP server to receive incoming connections from other clients.
class LocalClient : public Client
{
        Q_OBJECT
    public:
        LocalClient(Id<Client> id, QObject* parent = nullptr):
            Client{id, parent},
            m_server{new NetworkServer{9090, this}}
        {
            // todo : envoyer id et name du client.
            connect(m_server, SIGNAL(newSocket(QTcpSocket*)),
                    this, SIGNAL(createNewClient(QTcpSocket*)));
        }

        template<typename Deserializer>
        LocalClient(Deserializer&& vis, QObject* parent) :
            Client {vis, parent}
        {
        }


        int localPort()
        {
            return m_server->port();
        }

    signals:
        void createNewClient(QTcpSocket*);

    private:
        NetworkServer* m_server{};
};
