#include "RemoteActionReceiver.hpp"

#include <QApplication>
#include <core/application/Application.hpp>
#include <core/presenter/command/CommandQueue.hpp>

RemoteActionReceiver::RemoteActionReceiver(Session* s)
{
}

void RemoteActionReceiver::applyCommand(iscore::SerializableCommand* cmd)
{
	iscore::CommandQueue* queue = qApp->findChild<iscore::CommandQueue*>("CommandQueue");
	queue->push(cmd);
}

void RemoteActionReceiver::handle__edit_undo(osc::ReceivedMessageArgumentStream args)
{
	osc::int32 sessionId;
	osc::int32 clientId;

	args >> sessionId >> clientId;
	if(sessionId != session()->getId()) return;

	undo();
}

void RemoteActionReceiver::handle__edit_redo(osc::ReceivedMessageArgumentStream args)
{
	osc::int32 sessionId;
	osc::int32 clientId;

	args >> sessionId >> clientId;
	if(sessionId != session()->getId()) return;

	redo();
}

void RemoteActionReceiver::undo()
{
	iscore::CommandQueue* queue = qApp->findChild<iscore::CommandQueue*>("CommandQueue");
	queue->undo();
}

void RemoteActionReceiver::redo()
{
	iscore::CommandQueue* queue = qApp->findChild<iscore::CommandQueue*>("CommandQueue");
	queue->redo();
}

void RemoteActionReceiver::handle__edit_command(osc::ReceivedMessageArgumentStream args)
{
	osc::int32 sessionId;
	osc::int32 clientId;
	const char* par_name;
	const char* cmd_name;
	osc::Blob blob;

	args >> sessionId >> clientId >> par_name >> cmd_name >> blob;
	if(sessionId != session()->getId()) return;

	emit receivedCommand(QString{par_name},
						 QString{cmd_name},
						 QByteArray{static_cast<const char*>(blob.data), blob.size});
}
