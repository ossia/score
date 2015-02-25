#include "NetworkCommand.hpp"
#include <core/presenter/Presenter.hpp>

#include <Repartition/session/MasterSession.h>
#include <Repartition/client/RemoteClient.h>
#include <Repartition/session/ClientSessionBuilder.h>

#include "remote/RemoteActionEmitterMaster.hpp"
#include "remote/RemoteActionEmitterClient.hpp"

#include "remote/RemoteActionReceiverMaster.hpp"
#include "remote/RemoteActionReceiverClient.hpp"

#include "settings_impl/NetworkSettingsModel.hpp"

#include <QAction>
using namespace iscore;
NetworkCommand::NetworkCommand():
	PluginControlInterface{"NetworkCommand", nullptr}
{
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

void NetworkCommand::handle__document_ask(osc::ReceivedMessageArgumentStream args)
{
	osc::int32 sessionId, clientId;
	args >> sessionId >> clientId;

	if(sessionId != m_networkSession->getId()) return;

	auto dump = qApp->findChild<Document*>("Document")->save();

	osc::Blob blob{dump.constData(), dump.size()};
	m_networkSession->client(clientId).send("/document/receive", sessionId, blob);
}

void NetworkCommand::handle__document_receive(osc::ReceivedMessageArgumentStream args)
{
	osc::int32 sessionId;
	osc::Blob blob;
	args >> sessionId >> blob;

	if(sessionId != m_networkSession->getId()) return;

	QByteArray arr{(const char*)blob.data, blob.size};

	emit loadFromNetwork(arr);
}


//////////////////////////////////
void NetworkCommand::setupMasterSession()
{
	QSettings s;
	m_networkSession.reset();
	m_networkSession = std::make_unique<MasterSession>("Session Maitre", s.value(SETTINGS_MASTERPORT).toInt());

	auto session = static_cast<MasterSession*>(m_networkSession.get());
	session->getLocalMaster().receiver().addHandler("/document/ask",
										   &NetworkCommand::handle__document_ask,
										   this);

	m_emitter = std::make_unique<RemoteActionEmitter>(m_networkSession.get());
	m_emitter->setParent(this);
	m_receiver = std::make_unique<RemoteActionReceiverMaster>(this, session);
	m_receiver->setParent(this); // Else it does not work because childEvent is sent too early (only QObject is created)

	connect(m_receiver.get(), &RemoteActionReceiver::commandReceived,
			this,			  &NetworkCommand::on_commandReceived, Qt::QueuedConnection);
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
	session->getLocalClient().receiver().addHandler("/document/receive",
										 &NetworkCommand::handle__document_receive,
										 this);
	session->getRemoteMaster().send("/document/ask",
									session->getId(),
									session->getLocalClient().getId());

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

void NetworkCommand::commandPush(SerializableCommand* cmd)
{
	m_emitter->sendCommand(cmd);
}


#include <QApplication>
#include <core/application/Application.hpp>
#include <core/presenter/command/CommandQueue.hpp>
#include <core/interface/presenter/PresenterInterface.hpp>
void NetworkCommand::on_commandReceived(QString par_name,
										QString cmd_name,
										QByteArray data)
{
	auto cmd = iscore::IPresenter::instantiateUndoCommand(
										par_name,
										cmd_name,
										data);

	iscore::CommandQueue* queue = qApp->findChild<iscore::CommandQueue*>("CommandQueue");
	queue->push(cmd);
}
