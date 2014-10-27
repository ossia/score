#include "RemoteActionEmitterMaster.hpp"
#include <API/Headers/Repartition/session/MasterSession.h>
#include <core/presenter/command/Command.hpp>
#include <QDebug>

RemoteActionEmitterMaster::RemoteActionEmitterMaster(MasterSession* session):
	m_session{session}
{

}

void RemoteActionEmitterMaster::sendCommand(iscore::Command* cmd)
{
	QByteArray data = cmd->serialize();
	m_session->sendCommand(cmd->parentName().toStdString(),
						   cmd->name().toStdString(),
						   data.constData(),
						   data.length());

	qDebug() << "Master. Sending : " << cmd->parentName() << cmd->name();
}
