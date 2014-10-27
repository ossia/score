#pragma once
#include "RemoteActionReceiver.hpp"

class ClientSession;
class RemoteActionReceiverClient : public RemoteActionReceiver
{
		Q_OBJECT
	public:
		RemoteActionReceiverClient(QObject* parent, ClientSession*);
		virtual void onReceive(std::string, std::string, const char*, int) override;

	public slots:
		void applyCommand(iscore::Command*);

	private:
		ClientSession* m_session;
};
