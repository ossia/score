#pragma once
#include "Session.hpp"

class LocalClient;
class QObject;
class RemoteClient;
template <typename tag, typename impl> class id_base_t;

class ClientSession : public Session
{
    public:
        ClientSession(RemoteClient* master,
                      LocalClient* client,
                      Id<Session> id,
                      QObject* parent = nullptr);

        RemoteClient* master() const
        {
            return m_master;
        }

    private:
        RemoteClient* m_master{};
};

