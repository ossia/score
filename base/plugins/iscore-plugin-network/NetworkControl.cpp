#include "NetworkControl.hpp"
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentModel.hpp>

#include "DocumentPlugins/NetworkClientDocumentPlugin.hpp"
#include "DocumentPlugins/NetworkMasterDocumentPlugin.hpp"

#ifdef USE_ZEROCONF
#include "Zeroconf/ZeroconfBrowser.hpp"
#endif

#include "IpDialog.hpp"
#include <QApplication>
using namespace iscore;

NetworkControl::NetworkControl(Presenter* pres) :
    PluginControlInterface {pres, "NetworkControl", nullptr}
{
#ifdef USE_ZEROCONF
    m_zeroconfBrowser = new ZeroconfBrowser{this};
    connect(m_zeroconfBrowser, SIGNAL(sessionSelected(QString,int)),
            this, SLOT(setupClientConnection(QString, int)));
#endif

    setupCommands();
}

void NetworkControl::populateMenus(MenubarManager* menu)
{
#ifdef USE_ZEROCONF
    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                       FileMenuElement::Separator_Load,
                                       m_zeroconfBrowser->makeAction());
#endif

    QAction* makeServer = new QAction {tr("Make Server"), this};
    connect(makeServer, &QAction::triggered, this,
            [&] ()
    {
        auto clt = new LocalClient(id_type<Client>(0));
        clt->setName(tr("Master"));
        auto serv = new MasterSession(currentDocument(), clt, id_type<Session>(1234));
        auto policy = new MasterNetworkPolicy{serv, currentDocument()->commandStack(), currentDocument()->locker()};
        auto realplug = new NetworkDocumentPlugin{policy, currentDocument()->model()};
        currentDocument()->model()->addPluginModel(realplug);
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                       FileMenuElement::Separator_Load,
                                       makeServer);

    QAction* connectLocal = new QAction {tr("Join local"), this};
    connect(connectLocal, &QAction::triggered, this,
            [&] () {
        IpDialog dial{QApplication::activeWindow()};

        if(dial.exec())
        {
            // Default is 127.0.0.1 : 9090
            setupClientConnection(dial.ip(), dial.port());
        }
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                       FileMenuElement::Separator_Load,
                                       connectLocal);
}

#include "Repartition/session/ClientSessionBuilder.hpp"
void NetworkControl::setupClientConnection(QString ip, int port)
{
    m_sessionBuilder = new ClientSessionBuilder{
                       ip,
                       port};

    connect(m_sessionBuilder, &ClientSessionBuilder::sessionReady,
            this, &NetworkControl::on_sessionBuilt, Qt::QueuedConnection);

    m_sessionBuilder->initiateConnection();
}

void NetworkControl::on_sessionBuilt(
        ClientSessionBuilder* sessionBuilder,
        ClientSession* builtSession)
{
    // TODO Use RestoreDocument mechanism.
    /*
    auto doc = presenter()->loadDocument(
                   m_sessionBuilder->documentData(),
                   presenter()->availableDocuments().front());
    // TODO checkme

    // TODO use same mechanism than in presenter instead (CommandBackupFile).
    for(const auto& elt : m_sessionBuilder->commandStackData())
    {
        auto cmd = presenter()->instantiateUndoCommand(elt.first.first,
                                                       elt.first.second,
                                                       elt.second);
        doc->commandStack().pushQuiet(cmd);
    }

    auto np = static_cast<NetworkDocumentPlugin*>(doc->model()->pluginModel<NetworkDocumentPlugin>());
    np->setPolicy(new ClientNetworkPolicy{builtSession, doc});
    delete sessionBuilder;
    */
}


DocumentDelegatePluginModel *NetworkControl::loadDocumentPlugin(const QString &name,
                                                                const VisitorVariant &var,
                                                                iscore::DocumentModel *parent)
{
    if(name != NetworkDocumentPlugin::staticMetaObject.className())
        return nullptr;

    return new NetworkDocumentPlugin{var, parent};
}


#include "DistributedScenario/Commands/AddClientToGroup.hpp"
#include "DistributedScenario/Commands/RemoveClientFromGroup.hpp"

#include "DistributedScenario/Commands/CreateGroup.hpp"
#include "DistributedScenario/Commands/RemoveGroup.hpp"

#include "DistributedScenario/Commands/ChangeGroup.hpp"
#include <iscore/command/CommandGeneratorMap.hpp>

struct NetworkCommandFactory
{
        static CommandGeneratorMap map;
};

CommandGeneratorMap NetworkCommandFactory::map;

void NetworkControl::setupCommands()
{
    boost::mpl::for_each<
            boost::mpl::list<
                AddClientToGroup,
                RemoveClientFromGroup,
                CreateGroup,
                ChangeGroup
                // TODO RemoveGroup;
            >,
            boost::type<boost::mpl::_>
    >(CommandGeneratorMapInserter<NetworkCommandFactory>());
}

SerializableCommand* NetworkControl::instantiateUndoCommand(
        const QString& name,
        const QByteArray& data)
{
    return PluginControlInterface::instantiateUndoCommand<NetworkCommandFactory>(name, data);
}

