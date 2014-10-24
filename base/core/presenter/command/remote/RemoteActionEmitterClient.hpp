#pragma once
#include <core/presenter/command/remote/RemoteActionEmitter.hpp>

class ClientSession;
namespace iscore
{
	class RemoteActionEmitterClient : public RemoteActionEmitter
	{
		public:
			RemoteActionEmitterClient(ClientSession* session);

			virtual void sendCommand(Command*) override;

		private:
			ClientSession* m_session;
	};
}
