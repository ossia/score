#pragma once
#include "Session.hpp"

class ClientSession : public Session
{
	public:
		ClientSession(RemoteClient* master,
					  LocalClient* client,
					  id_type<Session> id,
					  QObject* parent = nullptr):
			Session{client, id, parent},
			m_master{master}
		{

		}

	private:
		RemoteClient* m_master{};
};

