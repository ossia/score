/*
#include "RemoteActionEmitterMaster.hpp"
#include <Repartition/session/MasterSession.h>
#include <iscore/command/SerializableCommand.hpp>
#include <QDebug>

RemoteActionEmitterMaster::RemoteActionEmitterMaster(MasterSession* session):
	m_session{session}
{

}

void RemoteActionEmitterMaster::sendCommand(iscore::SerializableCommand* cmd)
{
	QByteArray data = cmd->serialize();
	m_session->sendCommand(cmd->parentName().toStdString(),
						   cmd->name().toStdString(),
						   data.constData(),
						   data.length());

}
*/
