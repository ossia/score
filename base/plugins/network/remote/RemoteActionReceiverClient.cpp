#include "RemoteActionReceiverClient.hpp"
#include <API/Headers/Repartition/session/ClientSession.h>
#include <QDebug>

#include <QApplication>
#include <core/application/Application.hpp>
#include <core/presenter/command/CommandQueue.hpp>

RemoteActionReceiverClient::RemoteActionReceiverClient(QObject* parent, ClientSession* s):
	RemoteActionReceiver{},
	m_session{s}
{
	QObject* rec_parent = this;

	while(rec_parent != nullptr)
	{
		rec_parent = rec_parent->parent();
	}
	s->cmdCallback = std::bind(&RemoteActionReceiverClient::onReceive,
							   this,
							   std::placeholders::_1,
							   std::placeholders::_2,
							   std::placeholders::_3,
							   std::placeholders::_4);
}

void RemoteActionReceiverClient::onReceive(std::string parname, std::string name, const char* data, int len)
{
	// Instancier la bonne commande
	emit receivedCommand(QString::fromStdString(parname),
						 QString::fromStdString(name),
						 QByteArray{data, len});
}

void RemoteActionReceiverClient::applyCommand(iscore::Command* cmd)
{
	iscore::CommandQueue* queue = qApp->findChild<iscore::CommandQueue*>("CommandQueue");
	queue->push(cmd);
}
