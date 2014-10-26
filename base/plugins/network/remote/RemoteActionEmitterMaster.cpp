#include "RemoteActionEmitterMaster.hpp""
#include <API/Headers/Repartition/session/MasterSession.h>
#include <core/presenter/command/Command.hpp>
using namespace iscore;


RemoteActionEmitterMaster::RemoteActionEmitterMaster(MasterSession* session):
	m_session{session}
{

}


void RemoteActionEmitterMaster::sendCommand(Command* cmd)
{
	QByteArray data = cmd->serialize();
	m_session->sendCommand(cmd->name().toStdString(),
						   data.constData(),
						   data.length());
	qDebug("Master");
}
