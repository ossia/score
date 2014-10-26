#pragma once
#include "RemoteActionEmitter.hpp"

class MasterSession;
namespace iscore
{
	class RemoteActionEmitterMaster : public RemoteActionEmitter
	{
		public:
			RemoteActionEmitterMaster(MasterSession* session);

			virtual void sendCommand(Command*) override;

		private:
			MasterSession* m_session;
	};
}
