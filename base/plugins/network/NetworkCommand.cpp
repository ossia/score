#include "NetworkCommand.hpp"
#include <core/presenter/Presenter.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentModel.hpp>

#include "NetworkDocumentPlugin.hpp"
#include "Repartition/session/MasterSession.hpp"
#include "Serialization/NetworkServer.hpp"
#include "Serialization/NetworkSocket.hpp"
#include <QAction>


using namespace iscore;

NetworkControl::NetworkControl() :
    PluginControlInterface {"NetworkCommand", nullptr}
{
}

void NetworkControl::populateMenus(MenubarManager* menu)
{

 /*
    QAction* joinSession = new QAction {tr("Join"), this};
    connect(joinSession, &QAction::triggered,
            [&] () {
        createZeroconfSelectionDialog();
    });
    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                       FileMenuElement::Separator_Load,
                                       joinSession);
*/

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

void NetworkControl::populateToolbars()
{
}

#include "Repartition/session/ClientSessionBuilder.h"
void NetworkControl::setupClientConnection(QString ip, int port)
{
    m_sessionBuilder = new ClientSessionBuilder{
                ip,
                port};

    connect(m_sessionBuilder, &ClientSessionBuilder::sessionReady,
            this, &NetworkControl::on_sessionBuilt, Qt::QueuedConnection);

    m_sessionBuilder->doConnection();
}

void NetworkControl::on_sessionBuilt(ClientSessionBuilder* sessionBuilder, ClientSession* builtSession)
{
    auto doc = presenter()->loadDocument(
                   m_sessionBuilder->documentData(),
                   presenter()->availableDocuments().front());

    doc->model()->addPluginModel(new NetworkDocumentClientPlugin{builtSession, this, doc});

    delete sessionBuilder;

}

void NetworkControl::on_newDocument(Document* doc)
{
    // TODO FIXME
    //return;
    //doc->model()->addPluginModel(new NetworkDocumentPlugin{this, doc});
}
