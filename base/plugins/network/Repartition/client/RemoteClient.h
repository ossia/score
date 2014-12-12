#pragma once
#include "Client.h"
#include "RemoteSender.h"

class RemoteClient : public Client, public RemoteSender
{
	public:
		RemoteClient(const int id,
					 const std::string& hostname,
					 const std::string& remoteip,
					 const int remoteport):
			Client(id, hostname),
			RemoteSender(remoteip, remoteport)
		{
		}

		RemoteClient(const int id,
					 const std::string& hostname,
					 OscSender&& sender):
			Client(id, hostname),
			RemoteSender(std::move(sender))
		{
		}

		RemoteClient(RemoteClient&&) = default;
		RemoteClient(const RemoteClient&) = default;
		RemoteClient& operator=(const RemoteClient&) = default;
		RemoteClient& operator=(RemoteClient&&) = default;

		/**** Inter-client communication ****/
		// Cette méthode est appelée par le serveur.
		// Le serveur dit au client A (this, qui vient d'être créé)
		// d'initier la connection avec le client B (c).
		// Pour cela, on a besoin de l'ip de B par rapport au serveur.
		void initConnectionTo(int sessionId, RemoteClient& c)
		{
			if(c.getId() != getId())
			{
				send("/connect/discover",
					 sessionId,
					 c.getName().c_str(),
					 c.getId(),
					 c.ip().c_str(),
					 c.port());
			}
		}

		/**** Delay-related methods ****/
		void pollDelay();
		int getDelay();

		void setPingStamp(int ms)
		{
			_pingStamp = ms; // Faire struct & vecteur avec seqnum
		}

	private:
		int _delayInMs{}; // ns ? µs?
		int _pingStamp{};
};

