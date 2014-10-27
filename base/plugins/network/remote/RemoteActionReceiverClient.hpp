#pragma once
#include "RemoteActionReceiver.hpp"

#include <API/Headers/Repartition/session/ClientSession.h>

class ClientSession;
class RemoteActionReceiverClient : public RemoteActionReceiver
{
		Q_OBJECT
	public:
		RemoteActionReceiverClient(QObject* parent, ClientSession*);
		virtual void onReceive(std::string, std::string, const char*, int) override;

		void handle__edit_undo(osc::ReceivedMessageArgumentStream);
		void handle__edit_redo(osc::ReceivedMessageArgumentStream);
	public slots:
		void applyCommand(iscore::Command*);

	private:
		ClientSession* m_session;
};
