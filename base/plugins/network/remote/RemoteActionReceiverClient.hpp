#pragma once
#include "RemoteActionReceiver.hpp"
class ClientSession;
class RemoteActionReceiverClient : public RemoteActionReceiver
{
	public:
		RemoteActionReceiverClient(ClientSession*);
		virtual void onReceive(std::string, std::string, const char*, int) override;

	private:
		ClientSession* m_session;
};
