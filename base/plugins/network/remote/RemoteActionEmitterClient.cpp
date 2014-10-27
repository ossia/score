#include "RemoteActionEmitterClient.hpp"
#include <API/Headers/Repartition/session/ClientSession.h>

RemoteActionEmitterClient::RemoteActionEmitterClient(ClientSession* session):
	m_session{session}
{

}

void RemoteActionEmitterClient::sendCommand(iscore::Command*)
{
	qDebug("Client");
}
