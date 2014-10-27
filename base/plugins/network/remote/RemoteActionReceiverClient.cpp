#include "RemoteActionReceiverClient.hpp"
#include <QDebug>

#include <QApplication>
#include <core/application/Application.hpp>
#include <core/presenter/command/CommandQueue.hpp>

RemoteActionReceiverClient::RemoteActionReceiverClient(QObject* parent, ClientSession* s):
	RemoteActionReceiver{},
	m_session{s}
{
	s->cmdCallback = std::bind(&RemoteActionReceiverClient::onReceive,
							   this,
							   std::placeholders::_1,
							   std::placeholders::_2,
							   std::placeholders::_3,
							   std::placeholders::_4);

	s->getLocalClient().receiver().addHandler("/edit/undo",
										 &RemoteActionReceiverClient::handle__edit_undo,
										 this);
	s->getLocalClient().receiver().addHandler("/edit/redo",
										 &RemoteActionReceiverClient::handle__edit_redo,
										 this);
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

void RemoteActionReceiverClient::handle__edit_undo(osc::ReceivedMessageArgumentStream)
{
	iscore::CommandQueue* queue = qApp->findChild<iscore::CommandQueue*>("CommandQueue");
	queue->undo();
}

void RemoteActionReceiverClient::handle__edit_redo(osc::ReceivedMessageArgumentStream)
{
	iscore::CommandQueue* queue = qApp->findChild<iscore::CommandQueue*>("CommandQueue");
	queue->redo();
}
