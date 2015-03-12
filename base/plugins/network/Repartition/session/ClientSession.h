#pragma once
#include "Session.h"

class ClientSession : public Session
{
    public:
        ClientSession(LocalClient* master,
                      LocalClient* client,
                      id_type<Session> id,
                      QObject* parent):
            Session{client, id, parent},
            m_master{master}
        {

        }

    private:
        LocalClient* m_master{};
};

