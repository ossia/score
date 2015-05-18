#include "ClientSession.hpp"

ClientSession::ClientSession(RemoteClient* master,
                             LocalClient* client,
                             id_type<Session> id,
                             QObject* parent):
    Session{client, id, parent},
    m_master{master}
{
    addClient(master);
    connect(master, &RemoteClient::messageReceived,
            this, &Session::validateMessage, Qt::QueuedConnection);
}
