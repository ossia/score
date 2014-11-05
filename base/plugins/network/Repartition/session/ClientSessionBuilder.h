#pragma once
#include "ClientSession.h"
#include "ZeroConfClientThread.h"

class ClientSessionBuilder
{
	public:
		// à mettre en privé, devrait être appelé uniquement par ::list()
		ClientSessionBuilder(std::string masterIp,
							 int masterInputPort,
							 std::string localName,
							 int localPort):
			_session(new ClientSession("",
									   new RemoteMaster(0,
													   "",
													   masterIp,
													   masterInputPort),
									   new LocalClient(std::make_unique<OscReceiver>(localPort),
													   -1,
													   localName)))
		{
			_session->getLocalClient().receiver().addHandler("/session/clientNameIsTaken",
								 &ClientSessionBuilder::handle__session_clientNameIsTaken, this);

			_session->getLocalClient().receiver().addHandler("/session/setSessionId",
															 &ClientSessionBuilder::handle__session_setSessionId, this);
			_session->getLocalClient().receiver().addHandler("/session/setSessionName",
															 &ClientSessionBuilder::handle__session_setSessionName, this);
			_session->getLocalClient().receiver().addHandler("/session/setMasterName",
															 &ClientSessionBuilder::handle__session_setMasterName, this);

			_session->getLocalClient().receiver().addHandler("/session/setClientId",
															 &ClientSessionBuilder::handle__session_setClientId, this);
			_session->getLocalClient().receiver().addHandler("/session/update/group",
															 &ClientSessionBuilder::handle__session_update_group, this);

			_session->getLocalClient().receiver().addHandler("/session/isReady",
								 &ClientSessionBuilder::handle__session_isReady, this);

			_session->getLocalClient().receiver().run();
		}

		virtual ~ClientSessionBuilder() = default;
		ClientSessionBuilder(const ClientSessionBuilder& s) = default;
		ClientSessionBuilder(ClientSessionBuilder&& s) = default;

		/**** Handlers ****/
		// /session/update/group idSession idGroupe nomGroupe isMute
		void handle__session_update_group(osc::ReceivedMessageArgumentStream args)
		{
			osc::int32 idSession, idGroupe;
			bool isMute;
			const char* s;

			args >> idSession >> idGroupe >> s >> isMute >> osc::EndMessage;

			if(idSession == _session->getId())
			{
				auto& g = _session->private__createGroup(idGroupe, std::string(s));
				isMute? g.mute() : g.unmute();
			}
		}

		// /session/isReady idSession
		void handle__session_isReady(osc::ReceivedMessageArgumentStream args)
		{
			osc::int32 idSession;

			args >> idSession >> osc::EndMessage;

			if(_session->getId() == idSession) _isReady = true;
		}

		// /session/clientNameIsTaken idSession nom
		// TODO émettre message d'erreur dans thread Qt.
		void handle__session_clientNameIsTaken(osc::ReceivedMessageArgumentStream args)
		{
			osc::int32 idSession;
			const char* cname;

			args >> idSession >> cname >> osc::EndMessage;
			std::string name(cname);

			if(idSession == _session->getId() && _session->getLocalClient().getName() == name)
			{
				_session->getLocalClient().setName(_session->getLocalClient().getName() + "_");
				join();
			}
		}

		// /session/setSessionId idSession
		void handle__session_setSessionId(osc::ReceivedMessageArgumentStream args)
		{
			osc::int32 idSession;
			args >> idSession >> osc::EndMessage;

			_session->setId(idSession);
		}

		// /session/setSessionName idSession name
		void handle__session_setSessionName(osc::ReceivedMessageArgumentStream args)
		{
			osc::int32 idSession;
			const char* name;
			args >> idSession >> name >> osc::EndMessage;

			if(idSession == _session->getId())
			{
				_session->setName(name);
			}
		}

		void handle__session_setMasterName(osc::ReceivedMessageArgumentStream args)
		{
			osc::int32 idSession;
			const char* name;
			args >> idSession >> name >> osc::EndMessage;

			if(idSession == _session->getId())
			{
				_session->_remoteMaster->setName(name);
			}
		}

		// /session/setClientId idSession idClient
		void handle__session_setClientId(osc::ReceivedMessageArgumentStream args)
		{
			osc::int32 idSession, idClient;

			args >> idSession >> idClient >> osc::EndMessage;

			if(idSession == _session->getId())
			{
				LocalClient& lc = _session->getLocalClient();
				lc.setId(idClient);
			}
		}


		/**** Connection ****/
		// Si on veut, on récupère la liste des sessions dispo sur le réseau
		static std::vector<ConnectionData> list()
		{
			ZeroConfClientThread th;
			th.start();

			std::this_thread::sleep_for(std::chrono::seconds(2));

			std::vector<ConnectionData> vec = th.getData();
			th.exit();
			return vec;
		}

		// 2. On appelle "join" sur celle qu'on désire rejoindre.
		void join()
		{
			std::cerr << std::endl <<_session->getLocalClient().getName().c_str() << std::endl;
			std::cerr << _session->getLocalClient().localPort() << std::endl;
			_session->_remoteMaster->send("/session/connect",
								_session->getLocalClient().getName().c_str(),
								_session->getLocalClient().localPort());

			// Envoyer message de connection au serveur.
			// Il va construire peu à peu session.
			//
			// Eventuellement :
			//	/session/clientNameIsTaken
			//
			// Puis :
			//	/session/setClientId
			//	/session/update/group
			//
			// Enfin :
			//	/session/isReady
		}

		// 3. On teste si la construction est terminée
		// Construction : Envoyer tous les groupes.
		bool isReady()
		{
			return _isReady;
		}

		// 4. A appeler uniquement quand isReady
		std::unique_ptr<ClientSession>&& getBuiltSession()
		{
			if(!_isReady) throw "Is not ready";

			return std::move(_session);
		}

	private:
		std::unique_ptr<ClientSession> _session{nullptr};

		bool _isReady = false;
};
