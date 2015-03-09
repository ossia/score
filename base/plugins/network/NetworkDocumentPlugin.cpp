#include "NetworkDocumentPlugin.hpp"

#include <Repartition/session/MasterSession.h>
#include <Repartition/client/RemoteClient.h>
#include <Repartition/session/ClientSessionBuilder.h>

#include <Repartition/session/Session.h>

#include "remote/RemoteActionReceiverMaster.hpp"
#include "remote/RemoteActionReceiverClient.hpp"


#include <core/document/Document.hpp>
#include <core/document/DocumentPresenter.hpp>
#include "NetworkCommand.hpp"
#include "settings_impl/NetworkSettingsModel.hpp"


// TODO plutôt dans une vue ?
// TODO delete (mais pb avec threads...)
#include "zeroconf/ZeroConfConnectDialog.hpp"

NetworkDocumentPlugin::NetworkDocumentPlugin(NetworkControl *control, iscore::Document *doc):
    iscore::DocumentDelegatePluginModel{"NetworkDocumentPlugin", doc},
    m_control{control},
    m_document{doc}
{
    connect(&doc->commandStack(), &iscore::CommandStack::push_start,
            this, &NetworkDocumentPlugin::commandPush);

    setupMasterSession();
}


void NetworkDocumentPlugin::handle__document_ask(osc::ReceivedMessageArgumentStream args)
{
    osc::int32 sessionId, clientId;
    args >> sessionId >> clientId;

    if(sessionId != m_networkSession->getId())
    {
        return;
    }

    auto dump = m_control->currentDocument()->save();

    osc::Blob blob {dump.constData(), dump.size() };
    m_networkSession->client(clientId).send("/document/receive", sessionId, blob);
}


void NetworkDocumentPlugin::handle__document_receive(osc::ReceivedMessageArgumentStream args)
{
    osc::int32 sessionId;
    osc::Blob blob;
    args >> sessionId >> blob;

    if(sessionId != m_networkSession->getId())
    {
        return;
    }

    QByteArray arr {(const char*) blob.data, blob.size};

    // TODO instead, make the loading
    // from a tcp socket that would be opened prior to any document.
    // The "join" operation must not be in a created document;
    emit loadFromNetwork(arr);
}


void NetworkDocumentPlugin::setupMasterSession()
{
    QSettings s;
    m_networkSession.reset();
    m_networkSession = std::make_unique<MasterSession> ("Session Maitre",
                                                        s.value(SETTINGS_MASTERPORT).toInt());

    auto session = static_cast<MasterSession*>(m_networkSession.get());
    session->getLocalMaster().receiver().addHandler("/document/ask",
                                                    &NetworkDocumentPlugin::handle__document_ask,
                                                    this);

    m_emitter = std::make_unique<RemoteActionEmitter> (m_networkSession.get());
    m_emitter->setParent(this);
    m_receiver = std::make_unique<RemoteActionReceiverMaster> (this, session);
    m_receiver->setParent(this);  // Else it does not work because childEvent is sent too early (only QObject is created)

    connect(m_receiver.get(), &RemoteActionReceiver::commandReceived,
            this,			  &NetworkDocumentPlugin::on_commandReceived, Qt::QueuedConnection);

    setupConnections();
}


void NetworkDocumentPlugin::setupClientSession(ConnectionData d)
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

    while(!builder.isReady())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    m_networkSession.reset();
    m_networkSession = builder.getBuiltSession();

    auto session = static_cast<ClientSession*>(m_networkSession.get());
    session->getLocalClient().receiver().addHandler("/document/receive",
                                                    &NetworkDocumentPlugin::handle__document_receive,
                                                    this);
    session->getRemoteMaster().send("/document/ask",
                                    session->getId(),
                                    session->getLocalClient().getId());

    m_emitter = std::make_unique<RemoteActionEmitter> (m_networkSession.get());
    m_emitter->setParent(this);
    m_receiver = std::make_unique<RemoteActionReceiverClient> (this, session);
    m_receiver->setParent(this);  // Else it does not work because childEvent is sent too early.
    setupConnections();
}


void NetworkDocumentPlugin::createZeroconfSelectionDialog()
{
    auto diag = new ZeroconfConnectDialog();

    connect(diag,	&ZeroconfConnectDialog::connectedTo,
            this,	&NetworkDocumentPlugin::setupClientSession);

    diag->exec();
}


void NetworkDocumentPlugin::commandPush(iscore::SerializableCommand *cmd)
{
    m_emitter->sendCommand(cmd);
}


void NetworkDocumentPlugin::on_commandReceived(QString par_name, QString cmd_name, QByteArray data)
{
    auto cmd = m_control->presenter()->instantiateUndoCommand(
                par_name,
                cmd_name,
                data);

    CommandDispatcher<> cmdDispatcher(m_control->currentDocument());
    cmdDispatcher.submitCommand(cmd);
}


void NetworkDocumentPlugin::setupConnections()
{
    connect(&m_document->commandStack(), SIGNAL(onUndo()),
            m_emitter.get(), SLOT(undo()));
    connect(&m_document->commandStack(), SIGNAL(onRedo()),
            m_emitter.get(), SLOT(redo()));

    connect(m_receiver.get(), SIGNAL(undo()),
            &m_document->commandStack(), SLOT(undo()));
    connect(m_receiver.get(), SIGNAL(redo()),
            &m_document->commandStack(), SLOT(redo()));

    connect(m_document->presenter(), SIGNAL(lock(QByteArray)),
            m_emitter.get(), SLOT(on_lock(QByteArray)));
    connect(m_document->presenter(), SIGNAL(unlock(QByteArray)),
            m_emitter.get(), SLOT(on_unlock(QByteArray)));
}
