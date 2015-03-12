#pragma once
#include "../client/LocalClient.h"
#include "../client/RemoteClient.h"
#include <Serialization/NetworkMessage.hpp>
#include <Serialization/MessageMapper.hpp>
#include <Serialization/MessageValidator.hpp>


class Session : public IdentifiedObject<Session>
{
    public:
        Session(LocalClient* client, id_type<Session> id, QObject* parent = nullptr):
            IdentifiedObject<Session>{id, "Session", parent},
            m_client{client},
            m_mapper{new MessageMapper},
            m_validator{new MessageValidator(*this, *m_mapper)}
        {

        }

        LocalClient& localClient()
        {
            return *m_client;
        }

        void broadcast(NetworkMessage m)
        {
            for(RemoteClient* client : m_remoteClients)
                client->sendMessage(m);
        }


    protected:
        LocalClient* m_client{};

        MessageMapper* m_mapper{};
        MessageValidator* m_validator{};

        QList<RemoteClient*> m_remoteClients;

};

using Session_p = std::unique_ptr<Session>;
