#pragma once
#include "RemoteActionReceiver.hpp"

#include <API/Headers/Repartition/session/MasterSession.h>
class MasterSession;
class RemoteActionReceiverMaster : public RemoteActionReceiver
{
		Q_OBJECT
	public:
		RemoteActionReceiverMaster(QObject* parent, MasterSession*);
		virtual void onReceive(std::string, std::string, const char*, int) override;
		void handle__edit_undo(osc::ReceivedMessageArgumentStream);
		void handle__edit_redo(osc::ReceivedMessageArgumentStream);

	public slots:
		void applyCommand(iscore::Command*);

	private:
		MasterSession* m_session;
};
