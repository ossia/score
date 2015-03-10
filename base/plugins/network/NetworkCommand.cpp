#include "NetworkCommand.hpp"
#include <core/presenter/Presenter.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentModel.hpp>

#include <Repartition/session/MasterSession.h>
#include <Repartition/client/RemoteClient.h>
#include <Repartition/session/ClientSessionBuilder.h>

#include "remote/RemoteActionEmitter.hpp"

#include "remote/RemoteActionReceiverMaster.hpp"
#include "remote/RemoteActionReceiverClient.hpp"

#include "settings_impl/NetworkSettingsModel.hpp"

#include <QAction>

#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/command/OngoingCommandManager.hpp>

// TODO plutôt dans une vue ?
// TODO delete (mais pb avec threads...)
#include "zeroconf/ZeroConfConnectDialog.hpp"

using namespace iscore;
class NetworkDocumentPlugin : public iscore::DocumentDelegatePluginModel
{
    public:
        NetworkDocumentPlugin(NetworkControl* control, Document* doc):
            iscore::DocumentDelegatePluginModel{"NetworkDocumentPlugin", doc},
            m_control{control},
            m_document{doc}
        {
            connect(&doc->commandStack(), &CommandStack::push_start,
                    this, &NetworkDocumentPlugin::commandPush);

            setupMasterSession();
        }

        void handle__document_ask(osc::ReceivedMessageArgumentStream args)
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

        void handle__document_receive(osc::ReceivedMessageArgumentStream args)
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

    signals:
        void loadFromNetwork(QByteArray);

    public slots:
        void setupMasterSession()
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

        void setupClientSession(ConnectionData d)
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

        void createZeroconfSelectionDialog()
        {
            auto diag = new ZeroconfConnectDialog();

            connect(diag,	&ZeroconfConnectDialog::connectedTo,
                    this,	&NetworkDocumentPlugin::setupClientSession);

            diag->exec();
        }

        void commandPush(iscore::SerializableCommand* cmd)
        {
            m_emitter->sendCommand(cmd);
        }

        void on_commandReceived(QString par_name,
                                QString cmd_name,
                                QByteArray data)
        {
            auto cmd = m_control->presenter()->instantiateUndoCommand(
                           par_name,
                           cmd_name,
                           data);

            CommandDispatcher<> cmdDispatcher(m_control->currentDocument()->commandStack(), nullptr);
            cmdDispatcher.submitCommand(cmd);
        }



    private:
        void setupConnections()
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

        NetworkControl* m_control{};
        Document* m_document{};
        std::unique_ptr<Session> m_networkSession;
        std::unique_ptr<RemoteActionEmitter> m_emitter;
        std::unique_ptr<RemoteActionReceiver> m_receiver;

};

NetworkControl::NetworkControl() :
    PluginControlInterface {"NetworkCommand", nullptr}
{
}

void NetworkControl::populateMenus(MenubarManager* menu)
{
    QAction* joinSession = new QAction {tr("Join"), this};
    connect(joinSession, &QAction::triggered,
            [&] () {
        auto plug = static_cast<NetworkDocumentPlugin*>(
                        currentDocument()->model()->pluginModel("NetworkDocumentPlugin"));
        plug->createZeroconfSelectionDialog();
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                       FileMenuElement::Separator_Load,
                                       joinSession);
}

void NetworkControl::populateToolbars()
{
}

void NetworkControl::on_newDocument(Document* doc)
{
    // TODO FIXME
    return;
    doc->model()->addPluginModel(new NetworkDocumentPlugin{this, doc});
}
