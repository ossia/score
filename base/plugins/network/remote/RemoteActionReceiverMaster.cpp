#include "RemoteActionReceiverMaster.hpp"
#include <API/Headers/Repartition/session/MasterSession.h>
#include <QDebug>

#include <QApplication>
#include <core/application/Application.hpp>
#include <core/presenter/command/CommandQueue.hpp>

RemoteActionReceiverMaster::RemoteActionReceiverMaster(QObject* parent, MasterSession* s):
	RemoteActionReceiver{parent},
	m_session{s}
{
	this->setObjectName("RemoteActionReceiver");
	s->cmdCallback = std::bind(&RemoteActionReceiverMaster::onReceive,
							   this,
							   std::placeholders::_1,
							   std::placeholders::_2,
							   std::placeholders::_3,
							   std::placeholders::_4);
}


void RemoteActionReceiverMaster::onReceive(std::string parname, std::string name, const char* data, int len)
{
	// Instancier la bonne commande
	emit receivedCommand(QString::fromStdString(parname),
						 QString::fromStdString(name),
						 QByteArray{data, len});
}

void RemoteActionReceiverMaster::applyCommand(iscore::Command* cmd)
{
	iscore::CommandQueue* queue = qApp->findChild<iscore::CommandQueue*>("CommandQueue");
	queue->pushAndEmit(cmd);
}
