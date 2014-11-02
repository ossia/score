#pragma once
#include "RemoteActionEmitter.hpp"

class MasterSession;
class RemoteActionEmitterMaster : public RemoteActionEmitter
{
	public:
		RemoteActionEmitterMaster(MasterSession* session);

		virtual void sendCommand(iscore::Command*) override;

	private:
		MasterSession* m_session;
};
