#pragma once
#include "Session.hpp"

class ClientSession : public Session
{
    public:
        ClientSession(RemoteClient* master,
                      LocalClient* client,
                      id_type<Session> id,
                      QObject* parent = nullptr);

        RemoteClient* master() const
        {
            return m_master;
        }

    private:
        RemoteClient* m_master{};
};

