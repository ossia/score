#include "NetworkCommand.hpp"
#include <core/presenter/Presenter.hpp>

#include <Repartition/session/MasterSession.h>
#include <Repartition/session/ClientSessionBuilder.h>

#include "remote/RemoteActionEmitterMaster.hpp"
#include "remote/RemoteActionEmitterClient.hpp"

#include "remote/RemoteActionReceiverMaster.hpp"
#include "remote/RemoteActionReceiverClient.hpp"

#include "settings_impl/NetworkSettingsModel.hpp"

#include <QAction>
using namespace iscore;
NetworkCommand::NetworkCommand():
	PluginControlInterface{}
{
	this->setObjectName("NetworkCommand");
}

void NetworkCommand::populateMenus(MenubarManager* menu)
{
	QAction* joinSession = new QAction{tr("Join"), this};
	connect(joinSession, &QAction::triggered,
			this, &NetworkCommand::createZeroconfSelectionDialog);
	menu->insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
									   FileMenuElement::Separator_Load,
									   joinSession);
}

void NetworkCommand::populateToolbars()
{
}

void NetworkCommand::setPresenter(iscore::Presenter* pres)
{
	m_presenter = pres;
}


//////////////////////////////////
void NetworkCommand::setupMasterSession()
{
	QSettings s;
	m_networkSession.reset();
	m_networkSession = std::make_unique<MasterSession>("Session Maitre", s.value(SETTINGS_MASTERPORT).toInt());

	auto session = static_cast<MasterSession*>(m_networkSession.get());
	m_emitter = std::make_unique<RemoteActionEmitter>(m_networkSession.get());
	m_emitter->setParent(this);
	m_receiver = std::make_unique<RemoteActionReceiverMaster>(this, session);
	m_receiver->setParent(this); // Else it does not work because childEvent is sent too early (only QObject is created)
}

void NetworkCommand::setupClientSession(ConnectionData d)
{
	QSettings s;
	ClientSessionBuilder builder(d.remote_ip,
								 d.remote_port,
								 QString("%1%2")
									.arg(s.value(SETTINGS_CLIENTNAME).toString())
									.arg(generateRandom64())
									.toStdString(),
								 s.value(SETTINGS_CLIENTPORT).toInt());
	builder.join();
	while(!builder.isReady()) std::this_thread::sleep_for(std::chrono::milliseconds(50));
	m_networkSession.reset();
	m_networkSession = builder.getBuiltSession();

	auto session = static_cast<ClientSession*>(m_networkSession.get());
	m_emitter = std::make_unique<RemoteActionEmitter>(m_networkSession.get());
	m_emitter->setParent(this);
	m_receiver = std::make_unique<RemoteActionReceiverClient>(this, session);
	m_receiver->setParent(this);// Else it does not work because childEvent is sent too early.
}

// TODO plutôt dans une vue ?
// TODO delete (mais pb avec threads...)
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
