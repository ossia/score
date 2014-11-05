#include "RemoteActionReceiverMaster.hpp"
#include <Repartition/session/MasterSession.h>
#include <QDebug>

#include <QApplication>
#include <core/application/Application.hpp>
#include <core/presenter/command/CommandQueue.hpp>

RemoteActionReceiverMaster::RemoteActionReceiverMaster(QObject* parent, MasterSession* s):
	RemoteActionReceiver{s},
	m_session{s}
{
	s->getClient().receiver().addHandler("/edit/undo",
										 &RemoteActionReceiverMaster::handle__edit_undo,
										 this);
	s->getClient().receiver().addHandler("/edit/redo",
										 &RemoteActionReceiverMaster::handle__edit_redo,
										 this);

	s->getClient().receiver().addHandler("/edit/command",
										 &RemoteActionReceiverMaster::handle__edit_command,
										 this);
}

void RemoteActionReceiverMaster::handle__edit_command(osc::ReceivedMessageArgumentStream args)
{
	osc::int32 sessionId;
	osc::int32 clientId;
	const char* par_name;
	const char* cmd_name;
	osc::Blob blob;

	args >> sessionId >> clientId >> par_name >> cmd_name >> blob;
	if(sessionId != m_session->getId()) return;

	emit receivedCommand(QString{par_name},
						 QString{cmd_name},
						 QByteArray{static_cast<const char*>(blob.data), blob.size});

	for(auto& client : m_session->clients())
	{
		if(client.getId() != clientId)
		{
			client.send("/edit/command",
						sessionId,
						clientId,
						par_name,
						cmd_name,
						blob);
		}
	}
}

void RemoteActionReceiverMaster::handle__edit_undo(osc::ReceivedMessageArgumentStream args)
{
	osc::int32 sessionId;
	osc::int32 clientId;

	args >> sessionId >> clientId;
	if(sessionId != session()->getId()) return;

	undo();

	for(auto& client : m_session->clients())
	{
		if(client.getId() != clientId)
		{
			client.send("/edit/undo", sessionId, clientId);
		}
	}
}


void RemoteActionReceiverMaster::handle__edit_redo(osc::ReceivedMessageArgumentStream args)
{
	osc::int32 sessionId;
	osc::int32 clientId;

	args >> sessionId >> clientId;
	if(sessionId != session()->getId()) return;

	redo();

	for(auto& client : m_session->clients())
	{
		if(client.getId() != clientId)
		{
			client.send("/edit/redo", sessionId, clientId);
		}
	}

}
