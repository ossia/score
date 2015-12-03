#include <boost/optional/optional.hpp>
#include <core/application/Application.hpp>
#include <core/document/Document.hpp>
#include <QAction>
#include <QApplication>
#include <QDebug>
#include <qnamespace.h>

#include <QPair>
#include <algorithm>
#include <vector>

#include "DocumentPlugins/NetworkClientDocumentPlugin.hpp"
#include "DocumentPlugins/NetworkDocumentPlugin.hpp"
#include "DocumentPlugins/NetworkMasterDocumentPlugin.hpp"
#include "NetworkApplicationPlugin.hpp"
#include "Repartition/session/ClientSessionBuilder.hpp"

#include <iscore/application/ApplicationContext.hpp>
#include <core/command/CommandStack.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include "session/../client/LocalClient.hpp"
#include "session/MasterSession.hpp"

#ifdef USE_ZEROCONF
#include "Zeroconf/ZeroconfBrowser.hpp"
#endif

#include "IpDialog.hpp"

class Client;
class Session;
struct VisitorVariant;

using namespace iscore;

NetworkApplicationPlugin::NetworkApplicationPlugin(const iscore::ApplicationContext& app) :
    GUIApplicationContextPlugin {app, "NetworkApplicationPlugin", nullptr}
{
#ifdef USE_ZEROCONF
    m_zeroconfBrowser = new ZeroconfBrowser{"_iscore._tcp", qApp->activeWindow()};
    connect(m_zeroconfBrowser, SIGNAL(sessionSelected(QString,int)),
            this, SLOT(setupClientConnection(QString, int)));
#endif
}

void NetworkApplicationPlugin::populateMenus(MenubarManager* menu)
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
        auto clt = new LocalClient(Id<Client>(0));
        clt->setName(tr("Master"));
        auto serv = new MasterSession(currentDocument(), clt, Id<Session>(1234));
        auto policy = new MasterNetworkPolicy{serv, currentDocument()->context()};
        auto realplug = new NetworkDocumentPlugin{policy, *currentDocument()};
        currentDocument()->model().addPluginModel(realplug);
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

void NetworkApplicationPlugin::setupClientConnection(QString ip, int port)
{
    m_sessionBuilder = std::make_unique<ClientSessionBuilder>(
                context,
                ip,
                port);

    connect(m_sessionBuilder.get(), &ClientSessionBuilder::sessionReady,
            this, [&] () {
        m_sessionBuilder.reset();
    });
    connect(m_sessionBuilder.get(), &ClientSessionBuilder::sessionFailed,
            this, [&] () {
        m_sessionBuilder.reset();
    });

    m_sessionBuilder->initiateConnection();
}

DocumentPluginModel *NetworkApplicationPlugin::loadDocumentPlugin(const QString &name,
                                                                const VisitorVariant &var,
                                                                iscore::Document* parent)
{
    if(name != NetworkDocumentPlugin::staticMetaObject.className())
        return nullptr;

    return new NetworkDocumentPlugin{var, *parent};
}
