#include "NetworkCommand.hpp"
#include <core/presenter/Presenter.hpp>

#include <API/Headers/Repartition/session/MasterSession.h>
#include <API/Headers/Repartition/session/ClientSessionBuilder.h>

#include "remote/RemoteActionEmitterMaster.hpp"
#include "remote/RemoteActionEmitterClient.hpp"

#include "remote/RemoteActionReceiverMaster.hpp"
#include "remote/RemoteActionReceiverClient.hpp"

#include <QAction>
using namespace iscore;
NetworkCommand::NetworkCommand():
	CustomCommand{}
{
	this->setObjectName("NetworkCommand");
}

void NetworkCommand::populateMenus(MenubarManager* menu)
{
	QAction* joinSession = new QAction{tr("Join"), this};
	connect(joinSession, &QAction::triggered,
			this, &NetworkCommand::createZeroconfSelectionDialog);
	menu->insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
									   joinSession);
}

void NetworkCommand::populateToolbars()
{
}

void NetworkCommand::setPresenter(iscore::Presenter* pres)
{
	m_presenter = pres;
}

void NetworkCommand::setupMasterSession()
{
	m_networkSession = std::make_unique<MasterSession>("Session Maitre", 5678);

	auto session = static_cast<MasterSession*>(m_networkSession.get());
	m_emitter = std::make_unique<RemoteActionEmitterMaster>(session);
	//m_receiver = std::make_unique<RemoteActionReceiverMaster>(this, session);
}

void NetworkCommand::setupClientSession(ConnectionData d)
{
	ClientSessionBuilder builder(d.remote_ip, d.remote_port, "JeanMi", 7888);
	builder.join();
	sleep(2);
	m_networkSession = builder.getBuiltSession();

	auto session = static_cast<ClientSession*>(m_networkSession.get());
	m_emitter  = std::make_unique<RemoteActionEmitterClient>(session);
	m_receiver = std::make_unique<RemoteActionReceiverClient>(this, session);
	m_receiver->setParent(this);
}

#include "zeroconf/ZeroConfConnectDialog.hpp"
void NetworkCommand::createZeroconfSelectionDialog()
{
	auto diag = new ZeroconfConnectDialog();

	connect(diag,	&ZeroconfConnectDialog::connectedTo,
			this,	&NetworkCommand::setupClientSession);

	diag->exec();
}

void NetworkCommand::commandPush(Command* cmd)
{
	m_emitter->sendCommand(cmd);
}
