#pragma once

#include <ip/UdpSocket.h>
#include <osc/OscPacketListener.h>
#include <memory>
#include <thread>
#include <functional>
#include <map>
#include <iostream>

class OscReceiver
{
	public:
		using message_handler = std::function<void(osc::ReceivedMessageArgumentStream)>;
		using connection_handler = std::function<void(osc::ReceivedMessageArgumentStream, std::string)>;

		OscReceiver(unsigned int port)
		{
			setPort(port);
		}

		~OscReceiver()
		{
			socket->AsynchronousBreak();
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			_runThread.detach();
			socket.reset();
		}

		void run()
		{
			_runThread = std::thread(&UdpListeningReceiveSocket::Run, socket.get());
		}

		template<typename... K>
		void setConnectionHandler(K&&... args)
		{
			_impl.setConnectionHandler(std::forward<K>(args)...);
		}

		template<typename T, class C>
		void addHandler(const std::string &s, T&& theMember, C&& theObject)
		{
			_impl.addHandler(s, std::bind(theMember, theObject, std::placeholders::_1));
		}

		void addHandler(const std::string &s, const	message_handler h)
		{
			_impl.addHandler(s, h);
		}

		unsigned int port() const
		{
			return _port;
		}

		unsigned int setPort(unsigned int port)
		{
			_port = port;

			bool ok = false;
			while(!ok)
			{
				try
				{
					socket = std::make_shared<UdpListeningReceiveSocket>
							 (IpEndpointName(IpEndpointName::ANY_ADDRESS, _port),
							  &_impl);
					ok = true;
				}
				catch(std::runtime_error& e)
				{
					_port++;
				}
			}

			std::cerr << "Receiver port set : " << _port << std::endl;
			return _port;
		}

	private:
		unsigned int _port = 0;
		std::shared_ptr<UdpListeningReceiveSocket> socket;
		class : public osc::OscPacketListener
		{
			public:
				void setConnectionHandler(const std::string& s, const connection_handler& h)
				{
					_connectionAdress = s;
					_connectionHandler = h;
				}

				void addHandler(const std::string& s, const message_handler& h)
				{
					_map[s] = h;
				}

			protected:
				virtual void ProcessMessage(const osc::ReceivedMessage& m,
											const IpEndpointName& ip) override
				{
					try
					{
						auto addr = std::string(m.AddressPattern());
						std::cerr << "Message received on " << addr << std::endl;
						if(addr == _connectionAdress)
						{
							char s[16];
							ip.AddressAsString(s);
							_connectionHandler(m.ArgumentStream(), std::string(s));
						}
						else if(_map.find(addr) != _map.end())
						{
							_map[addr](m.ArgumentStream());
						}
					}
					catch( osc::Exception& e )
					{
						std::cerr << "error while parsing message: "
								  << m.AddressPattern() << ": " << e.what() << std::endl;
					}
				}

			private:
				std::map<std::string, message_handler> _map;
				std::string _connectionAdress{};
				connection_handler _connectionHandler{};
		} _impl{};

		std::thread _runThread;
};
