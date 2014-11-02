#pragma once
#include "RemoteActionReceiver.hpp"

#include <API/Headers/Repartition/session/ClientSession.h>

class ClientSession;
class RemoteActionReceiverClient : public RemoteActionReceiver
{
		Q_OBJECT
	public:
		RemoteActionReceiverClient(QObject* parent, ClientSession*);

	protected:
		virtual Session* session() override
		{ return m_session; }

	private:
		ClientSession* m_session;
};
