#pragma once
#include <core/tools/utilsCPP11.hpp>
#include <interface/plugincontrol/PluginControlInterface.hpp>

#include <Repartition/session/Session.h>
#include <Repartition/session/ConnectionData.hpp>
#include <memory>

#include "remote/RemoteActionEmitter.hpp"
#include "remote/RemoteActionReceiver.hpp"


#include <core/presenter/command/Command.hpp>

class NetworkCommand : public iscore::PluginControlInterface
{
		Q_OBJECT

	public:
		NetworkCommand();
		virtual void populateMenus(iscore::MenubarManager*) override;
		virtual void populateToolbars() override;
		virtual void setPresenter(iscore::Presenter*) override;

	public slots:
		void setupMasterSession();
		void setupClientSession(ConnectionData d);

		void createZeroconfSelectionDialog();

		void commandPush(iscore::SerializableCommand*);

	private:
		iscore::Presenter* m_presenter{};
		std::unique_ptr<Session> m_networkSession;

	private:
		std::unique_ptr<RemoteActionEmitter> m_emitter;
		std::unique_ptr<RemoteActionReceiver> m_receiver;
};
