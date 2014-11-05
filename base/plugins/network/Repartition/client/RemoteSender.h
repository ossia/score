#pragma once
#include "../osc/oscsender.h"
#include "../osc/oscmessagegenerator.h"

class RemoteSender
{
	public:
		RemoteSender(const std::string& ip, const int port):
			_sender(ip, port)
		{
		}

		RemoteSender(OscSender&& sender):
			_sender(std::move(sender))
		{
		}

		virtual ~RemoteSender() = default;

		// Emission de données vers ce client
		void send(const osc::OutboundPacketStream& s)
		{
			_sender.send(s);
		}

		void send(const osc::MessageGenerator& m)
		{
			_sender.send(m.stream());
		}

		template<typename... T>
		void send(const std::string& name, const T... args)
		{
			send(osc::MessageGenerator(name, args...));
		}

		std::string ip()
		{
			return _sender.ip();
		}

		int port()
		{
			return _sender.port();
		}

	private:
		OscSender _sender;
};
