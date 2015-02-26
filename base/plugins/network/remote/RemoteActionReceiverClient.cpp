#include "RemoteActionReceiverClient.hpp"
#include <QDebug>


RemoteActionReceiverClient::RemoteActionReceiverClient (QObject* parent, ClientSession* s) :
    RemoteActionReceiver {s},
m_session {s}
{
    s->getClient().receiver().addHandler ("/edit/undo",
    &RemoteActionReceiverClient::handle__edit_undo,
    this);
    s->getClient().receiver().addHandler ("/edit/redo",
    &RemoteActionReceiverClient::handle__edit_redo,
    this);

    s->getClient().receiver().addHandler ("/edit/lock",
    &RemoteActionReceiverClient::handle__edit_lock,
    this);
    s->getClient().receiver().addHandler ("/edit/unlock",
    &RemoteActionReceiverClient::handle__edit_unlock,
    this);

    s->getClient().receiver().addHandler ("/edit/command",
    &RemoteActionReceiverClient::handle__edit_command,
    this);
}
