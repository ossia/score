/*
#include "RemoteActionEmitterClient.hpp"
#include <Repartition/session/ClientSession.h>
#include <public_interface/command/Command.hpp>
RemoteActionEmitterClient::RemoteActionEmitterClient(ClientSession* session):
	m_session{session}
{

}

void RemoteActionEmitterClient::sendCommand(iscore::SerializableCommand* cmd)
{
	QByteArray data = cmd->serialize();
	m_session->sendCommand(cmd->parentName().toStdString(),
						   cmd->name().toStdString(),
						   data.constData(),
						   data.length());

}

void RemoteActionEmitterClient::undo()
{
	m_session->undoCommand
}

void RemoteActionEmitterClient::redo()
{

}
*/
