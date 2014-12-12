#pragma once
#include "RemoteActionEmitter.hpp"

class ClientSession;
class RemoteActionEmitterClient : public RemoteActionEmitter
{
	public:
		RemoteActionEmitterClient(ClientSession* session);

		virtual void sendCommand(iscore::SerializableCommand*) override;
		virtual void undo() override;
		virtual void redo() override;

	private:
		ClientSession* m_session;
};
