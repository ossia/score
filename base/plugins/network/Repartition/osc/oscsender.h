#pragma once
#include <ip/UdpSocket.h>
#include <osc/OscOutboundPacketStream.h>

#include <string>
#include <iostream>
#include <memory>

class OscSenderInterface
{
	public:
		virtual void send(const osc::OutboundPacketStream&) = 0;
		virtual ~OscSenderInterface() = default;
};

// Faire simple et multicast,
class OscSender: public OscSenderInterface
{
	public:
		OscSender(const std::string& ip, const int port):
			transmitSocket{std::make_shared<UdpTransmitSocket>(IpEndpointName(ip.c_str(), port))},
			_ip(ip),
			_port(port)
		{
		}

		virtual ~OscSender() = default;
		OscSender(OscSender&&) = default;
		OscSender(const OscSender&) = delete;
		OscSender& operator=(const OscSender&) = delete;

		virtual void send(const osc::OutboundPacketStream& m) override
		{
			std::cerr << "to : " << _ip << ":" << _port << std::endl;
			transmitSocket->Send( m.Data(), m.Size() );
		}

		std::string ip() { return _ip; }
		int port() { return _port; }

	private:
		std::shared_ptr<UdpTransmitSocket> transmitSocket;
		std::string _ip;
		int _port;
};
