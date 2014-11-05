#pragma once
#include "Client.h"

class LocalClient : public Client
{
	friend class ClientSessionBuilder;
	using SignalHandler = std::function<void()>;

	public:
		LocalClient(int port, int id, std::string name):
			Client(id, name),
			_receiver(new OscReceiver(port))
		{
		}

		LocalClient(std::unique_ptr<OscReceiver>&& receiver, int id, std::string name):
			Client(id, name),
			_receiver(std::move(receiver))
		{
		}

		virtual ~LocalClient() = default;

		int localPort() const
		{
			return _receiver->port();
		}

		void setLocalPort(unsigned int c)
		{
			_receiver->setPort(c);
		}

		OscReceiver& receiver()
		{
			return *_receiver;
		}

	protected:
		std::unique_ptr<OscReceiver> _receiver{nullptr};
};
