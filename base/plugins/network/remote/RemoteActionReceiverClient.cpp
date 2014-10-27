#include "RemoteActionReceiverClient.hpp"
#include <QDebug>


RemoteActionReceiverClient::RemoteActionReceiverClient(QObject* parent, ClientSession* s):
	RemoteActionReceiver{s},
	m_session{s}
{
	s->getClient().receiver().addHandler("/edit/undo",
										 &RemoteActionReceiverClient::handle__edit_undo,
										 this);
	s->getClient().receiver().addHandler("/edit/redo",
										 &RemoteActionReceiverClient::handle__edit_redo,
										 this);

	s->getClient().receiver().addHandler("/edit/command",
										 &RemoteActionReceiverClient::handle__edit_command,
										 this);
}



