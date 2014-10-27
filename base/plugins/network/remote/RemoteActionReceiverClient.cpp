#include "RemoteActionReceiverClient.hpp"
#include <API/Headers/Repartition/session/ClientSession.h>
#include <QDebug>
RemoteActionReceiverClient::RemoteActionReceiverClient(ClientSession* s):
	m_session{s}
{
	s->cmdCallback = std::bind(&RemoteActionReceiverClient::onReceive,
							   this,
							   std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
}

void RemoteActionReceiverClient::onReceive(std::string parname, std::string name, const char* data, int len)
{
	QByteArray{data, len};

	qDebug() << "NAME:" << parname.c_str() << name.c_str();

	// Instancier la bonne commande

}
