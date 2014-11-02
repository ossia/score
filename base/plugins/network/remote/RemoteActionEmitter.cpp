#include "RemoteActionEmitter.hpp"
#include <API/Headers/Repartition/session/Session.h>
#include <core/presenter/command/Command.hpp>

RemoteActionEmitter::RemoteActionEmitter(Session* session):
	m_session{session}
{

}

void RemoteActionEmitter::sendCommand(iscore::Command* cmd)
{
	QByteArray data = cmd->serialize();
	m_session->sendCommand(cmd->parentName().toLatin1().constData(),
						   cmd->name().toLatin1().constData(),
						   data.constData(),
						   data.length());

}

void RemoteActionEmitter::undo()
{
	m_session->sendUndoCommand();
}

void RemoteActionEmitter::redo()
{
	m_session->sendRedoCommand();
}
