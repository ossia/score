#include "NetworkControl.hpp"
#include <core/presenter/Presenter.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentModel.hpp>

#include "NetworkDocumentPlugin.hpp"
#include "Repartition/session/ClientSession.hpp"
#include "Repartition/session/MasterSession.hpp"
#include "Serialization/NetworkServer.hpp"
#include "Serialization/NetworkSocket.hpp"
#include <QAction>

#ifdef USE_ZEROCONF
#include "Zeroconf/ZeroconfBrowser.hpp"
#endif


using namespace iscore;

NetworkControl::NetworkControl() :
    PluginControlInterface {"NetworkCommand", nullptr}
{
#ifdef USE_ZEROCONF
    m_zeroconfBrowser = new ZeroconfBrowser{this};
    connect(m_zeroconfBrowser, SIGNAL(sessionSelected(QString,int)),
            this, SLOT(setupClientConnection(QString, int)));
#endif
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
        auto serv = new MasterSession(currentDocument(), clt, id_type<Session>(1234));
        auto plug = new NetworkDocumentMasterPlugin{serv, this, currentDocument()};
        currentDocument()->model()->addPluginModel(plug);
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                       FileMenuElement::Separator_Load,
                                       makeServer);

    QAction* connectLocal = new QAction {tr("Join local"), this};
    connect(connectLocal, &QAction::triggered, this,
            [&] ()
    {
        setupClientConnection("127.0.0.1", 9090);
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

void NetworkControl::on_sessionBuilt(ClientSessionBuilder* sessionBuilder, ClientSession* builtSession)
{
    auto doc = presenter()->loadDocument(
                   m_sessionBuilder->documentData(),
                   presenter()->availableDocuments().front());

    for(const auto& elt : m_sessionBuilder->commandStackData())
    {
        auto cmd = presenter()->instantiateUndoCommand(elt.first.first, elt.first.second, elt.second);
        doc->commandStack().pushQuiet(cmd);
    }
    doc->model()->addPluginModel(new NetworkDocumentClientPlugin{builtSession, this, doc});

    delete sessionBuilder;

}
