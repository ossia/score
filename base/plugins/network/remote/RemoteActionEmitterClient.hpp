#pragma once
#include "RemoteActionEmitter.hpp"

class ClientSession;
class RemoteActionEmitterClient : public RemoteActionEmitter
{
	public:
		RemoteActionEmitterClient(ClientSession* session);

		virtual void sendCommand(iscore::Command*) override;

	private:
		ClientSession* m_session;
};
