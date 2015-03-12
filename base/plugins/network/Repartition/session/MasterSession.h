#pragma once
#include "Session.h"
#include "../client/LocalMaster.h"
#include "ZeroConfServerThread.h"

class MasterSession : public Session
{
        ZeroConfServerThread _zc_thread;
    public:
        MasterSession(LocalClient* client,
                      id_type<Session> id,
                      QObject* parent):
            Session{client, id, parent}
        {

        }

        virtual ~MasterSession()
        {
            _zc_thread.quit();
            while(_zc_thread.isRunning()) std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        void transmit(id_type<RemoteClient> sender, NetworkMessage m)
        {
            for(auto& client : m_remoteClients)
            {
                if(client->id() != sender)
                    client->sendMessage(m);
            }
        }

};
