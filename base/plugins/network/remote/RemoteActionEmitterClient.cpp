#include "RemoteActionEmitterClient.hpp"
#include <API/Headers/Repartition/session/ClientSession.h>

using namespace iscore;

RemoteActionEmitterClient::RemoteActionEmitterClient(ClientSession* session):
	m_session{session}
{

}

void RemoteActionEmitterClient::sendCommand(Command*)
{
	qDebug("Client");
}
