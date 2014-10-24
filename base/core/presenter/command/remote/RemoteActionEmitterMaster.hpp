#pragma once
#include <core/presenter/command/remote/RemoteActionEmitter.hpp>

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
