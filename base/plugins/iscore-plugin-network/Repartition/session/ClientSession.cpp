#include <boost/optional/optional.hpp>
#include <qnamespace.h>

#include "ClientSession.hpp"
#include "iscore/tools/SettableIdentifier.hpp"
#include "session/../client/RemoteClient.hpp"
#include "session/Session.hpp"

class LocalClient;
class QObject;

ClientSession::ClientSession(RemoteClient* master,
                             LocalClient* client,
                             Id<Session> id,
                             QObject* parent):
    Session{client, id, parent},
    m_master{master}
{
    addClient(master);
    connect(master, &RemoteClient::messageReceived,
            this, &Session::validateMessage, Qt::QueuedConnection);
}
