#pragma once
#include "Client.h"
#include "RemoteSender.h"
#include <Serialization/NetworkSocket.hpp>

// Has a TCP socket for exchange with this client.
class RemoteClient : public Client
{
    public:
        RemoteClient(NetworkSocket* socket,
                     id_type<Client> id,
                     QObject* parent = nullptr):
            Client(id, parent),
            m_socket{socket}
        {
            connect(m_socket, SIGNAL(messageReceived(NetworkMessage)),
                    this,     SIGNAL(messageReceived(NetworkMessage)));
        }

        void sendMessage(NetworkMessage m)
        {
            m_socket->sendMessage(m);
        }

    private:
        NetworkSocket* m_socket{};
};

