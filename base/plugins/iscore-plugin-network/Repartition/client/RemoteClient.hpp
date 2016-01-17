#pragma once
#include "Client.hpp"
#include <Serialization/NetworkSocket.hpp>
namespace Network
{
// Has a TCP socket for exchange with this client.
class RemoteClient : public Client
{
        Q_OBJECT
    public:
        RemoteClient(NetworkSocket* socket,
                     Id<Client> id,
                     QObject* parent = nullptr):
            Client(id, parent),
            m_socket{socket}
        {
            connect(m_socket, SIGNAL(messageReceived(NetworkMessage)),
                    this,     SIGNAL(messageReceived(NetworkMessage)));
        }

        template<typename Deserializer>
        RemoteClient(Deserializer&& vis, QObject* parent) :
            Client {vis, parent}
        {
        }

        void sendMessage(const NetworkMessage& m)
        {
            m_socket->sendMessage(m);
        }

    signals:
        void messageReceived(NetworkMessage);

    private:
        NetworkSocket* m_socket{};
};

}
