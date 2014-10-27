#pragma once
#include "RemoteActionReceiver.hpp"
class MasterSession;
class RemoteActionReceiverMaster : public RemoteActionReceiver
{
		Q_OBJECT
	public:
		RemoteActionReceiverMaster(QObject* parent, MasterSession*);
		virtual void onReceive(std::string, std::string, const char*, int) override;

	public slots:
		void applyCommand(iscore::Command*);

	private:
		MasterSession* m_session;
};
