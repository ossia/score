#pragma once
#include "RemoteClient.h"
#include "../Iterable.h"

class ClientManager : public Iterable<RemoteClient>
{
	private:
		int _lastId = 0;

	public:
		RemoteClient& createConnection(int id,
									   std::string hostname,
									   const std::string& ip,
									   const int port)
		{
			performUniquenessCheck(hostname);
			performUniquenessCheck(id);

			_lastId = std::max(id, _lastId);

			return create(id,
						  hostname,
						  ip,
						  port);
		}

		RemoteClient& createConnection(std::string hostname,
									   const std::string& ip,
									   const int port)
		{
			performUniquenessCheck(hostname);
			return create(++_lastId,
						  hostname,
						  ip,
						  port);
		}

		RemoteClient& createConnection(std::string hostname,
									   OscSender&& sender)
		{
			performUniquenessCheck(hostname);
			return create(++_lastId,
						  hostname,
						  std::move(sender));
		}
};
